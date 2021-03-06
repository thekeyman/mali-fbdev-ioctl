/* This file is part of mali-fbdev-ioctl.
 * Copyright (C) 2014-2015 - Tobias Jakobi
 *
 * mali-fbdev-ioctl is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * mali-fbdev-ioctl is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with mali-fbdev-ioctl. If not, see <http://www.gnu.org/licenses/>.
 */

#include "common.h"

#include <stdlib.h>
#include <errno.h>

#if MALI_VERSION == 0x0400
  #include "mali_ioctl_r4p0.h"
#elif MALI_VERSION == 0x0500
  #include "mali_ioctl_r5p0.h"
#else
  #error "Unsupported Mali version requested!"
#endif

static struct hook_data hook = {
  .fbdev_fd = -1,
  .mali_fd = -1,
  .drm_fd = -1,

  .open = NULL,
  .close = NULL,
  .ioctl = NULL,
  .mmap = NULL,
  .munmap = NULL,

  .width = 0,
  .height = 0,
  .pitch = 0,
  .bpp = 0,
  .size = 0,

  .fake_vscreeninfo = NULL,
  .fake_fscreeninfo = NULL,
  .base_addr = 0,
  .fake_mmap = NULL,
  .initialized = 0,

  .device = NULL,
  .drm = NULL,
  .fliphandler = NULL,

  .pages = NULL,
  .num_pages = 0,
  .cur_page = NULL,
  .pageflip_pending = 0
};

static hsetupfnc hinit = NULL;
static hsetupfnc hfree = NULL;
static hflipfnc hflip = NULL;
static hbufferfnc hbuffer = NULL;

void setup_hook_callback(hsetupfnc init_, hsetupfnc free_,
  hflipfnc flip_, hbufferfnc buffer_) {
#ifdef HOOK_VERBOSE
  fprintf(stderr, "info: setup_hook_callback called\n");
#endif

  hinit = init_;
  hfree = free_;
  hflip = flip_;
  hbuffer = buffer_;
}

int hook_get_drm_fd() {
  return hook.drm_fd;
}

static const char* translate_mali_ioctl(unsigned long request) {
  switch (request) {
   case MALI_IOC_WAIT_FOR_NOTIFICATION:
      return "MALI_IOC_WAIT_FOR_NOTIFICATION";
   case MALI_IOC_GET_API_VERSION:
      return "MALI_IOC_GET_API_VERSION";
   case MALI_IOC_GET_API_VERSION_V2:
      return "MALI_IOC_GET_API_VERSION_V2";
   case MALI_IOC_POST_NOTIFICATION:
      return "MALI_IOC_POST_NOTIFICATION";
   case MALI_IOC_GET_USER_SETTING:
      return "MALI_IOC_GET_USER_SETTING";
   case MALI_IOC_GET_USER_SETTINGS:
      return "MALI_IOC_GET_USER_SETTINGS";
   case MALI_IOC_REQUEST_HIGH_PRIORITY:
      return "MALI_IOC_REQUEST_HIGH_PRIORITY";
   case MALI_IOC_TIMELINE_GET_LATEST_POINT:
      return "MALI_IOC_TIMELINE_GET_LATEST_POINT";
   case MALI_IOC_TIMELINE_WAIT:
      return "MALI_IOC_TIMELINE_WAIT";
   case MALI_IOC_TIMELINE_CREATE_SYNC_FENCE:
      return "MALI_IOC_TIMELINE_CREATE_SYNC_FENCE";
   case MALI_IOC_SOFT_JOB_START:
      return "MALI_IOC_SOFT_JOB_START";
   case MALI_IOC_SOFT_JOB_SIGNAL:
      return "MALI_IOC_SOFT_JOB_SIGNAL";
   case MALI_IOC_MEM_MAP_EXT:
      return "MALI_IOC_MEM_MAP_EXT";
   case MALI_IOC_MEM_UNMAP_EXT:
      return "MALI_IOC_MEM_UNMAP_EXT";
   case MALI_IOC_MEM_ATTACH_DMA_BUF:
      return "MALI_IOC_MEM_ATTACH_DMA_BUF";
   case MALI_IOC_MEM_RELEASE_DMA_BUF:
      return "MALI_IOC_MEM_RELEASE_DMA_BUF";
   case MALI_IOC_MEM_DMA_BUF_GET_SIZE:
      return "MALI_IOC_MEM_DMA_BUF_GET_SIZE";
   case MALI_IOC_MEM_QUERY_MMU_PAGE_TABLE_DUMP_SIZE:
      return "MALI_IOC_MEM_QUERY_MMU_PAGE_TABLE_DUMP_SIZE";
   case MALI_IOC_MEM_DUMP_MMU_PAGE_TABLE:
      return "MALI_IOC_MEM_DUMP_MMU_PAGE_TABLE";
   case MALI_IOC_MEM_WRITE_SAFE:
      return "MALI_IOC_MEM_WRITE_SAFE";
   case MALI_IOC_PP_START_JOB:
      return "MALI_IOC_PP_START_JOB";
   case MALI_IOC_PP_AND_GP_START_JOB:
      return "MALI_IOC_PP_AND_GP_START_JOB";
   case MALI_IOC_PP_NUMBER_OF_CORES_GET:
      return "MALI_IOC_PP_NUMBER_OF_CORES_GET";
   case MALI_IOC_PP_CORE_VERSION_GET:
      return "MALI_IOC_PP_CORE_VERSION_GET";
   case MALI_IOC_PP_DISABLE_WB:
      return "MALI_IOC_PP_DISABLE_WB";
   case MALI_IOC_GP2_START_JOB:
      return "MALI_IOC_GP2_START_JOB";
   case MALI_IOC_GP2_NUMBER_OF_CORES_GET:
      return "MALI_IOC_GP2_NUMBER_OF_CORES_GET";
   case MALI_IOC_GP2_CORE_VERSION_GET:
      return "MALI_IOC_GP2_CORE_VERSION_GET";
   case MALI_IOC_GP2_SUSPEND_RESPONSE:
      return "MALI_IOC_GP2_SUSPEND_RESPONSE";
   case MALI_IOC_PROFILING_ADD_EVENT:
      return "MALI_IOC_PROFILING_ADD_EVENT";
   case MALI_IOC_PROFILING_REPORT_SW_COUNTERS:
      return "MALI_IOC_PROFILING_REPORT_SW_COUNTERS";
   case MALI_IOC_PROFILING_MEMORY_USAGE_GET:
      return "MALI_IOC_PROFILING_MEMORY_USAGE_GET";
   case MALI_IOC_VSYNC_EVENT_REPORT:
      return "MALI_IOC_VSYNC_EVENT_REPORT";
   default:
      return "UNKNOWN";
  }
}

static int emulate_get_var_screeninfo(void *ptr) {
#ifdef HOOK_VERBOSE
  fprintf(stderr, "info: emulate_get_var_screeninfo called\n");
#endif

  if (hook.fake_vscreeninfo) {
    memcpy(ptr, hook.fake_vscreeninfo, sizeof(struct fb_var_screeninfo));
    return 0;
  } else {
    return -ENOTTY;
  }
}

static int emulate_put_var_screeninfo(void *ptr) {
#ifndef QUIET_NONIMPLEMENTED
  fprintf(stderr, "info: emulate_put_var_screeninfo called\n");
#endif

  /* TODO: implement */
  return -ENOTTY;
}

static int emulate_get_fix_screeninfo(void *ptr) {
#ifdef HOOK_VERBOSE
  fprintf(stderr, "info: emulate_get_fix_screeninfo called\n");
#endif

  if (hook.fake_fscreeninfo) {
    memcpy(ptr, hook.fake_fscreeninfo, sizeof(struct fb_fix_screeninfo));
    return 0;
  } else {
    return -ENOTTY;
  }
}

static int emulate_pan_display(void *ptr) {
  const struct fb_var_screeninfo *data = ptr;
  int ret = -ENOTTY;

  if (data->xoffset == 0 && ((data->yoffset % hook.height) == 0))
    ret = hflip(&hook, data->yoffset / hook.height);

  return ret;
}

static int emulate_waitforvsync(void *ptr) {
#ifndef QUIET_NONIMPLEMENTED
  fprintf(stderr, "info: emulate_waitforvsync called\n");
#endif

  /* TODO: implement */
  return -ENOTTY;
}

static int emulate_get_fb_dma_buf(void *ptr) {
#ifndef QUIET_NONIMPLEMENTED
  fprintf(stderr, "info: emulate_get_fb_dma_buf called\n");
#endif

  /* TODO: implement */
  return -ENOTTY;
}

static int emulate_mali_mem_map_ext(void *ptr) {
#ifdef HOOK_VERBOSE
  fprintf(stderr, "info: emulate_mali_mem_map_ext called\n");
#endif

  _mali_uk_map_external_mem_s *data = ptr;
  int buf_fd = -1;

  if (hook.base_addr <= data->phys_addr) {
    const unsigned long offset = data->phys_addr - hook.base_addr;

    if ((offset % hook.size) == 0)
      buf_fd = hbuffer(&hook, offset / hook.size);
  }

  if (buf_fd != -1) {
#ifdef HOOK_VERBOSE
    fprintf(stderr, "info: translating to dma-buf attach\n");
#endif

    _mali_uk_attach_dma_buf_s newdata = { 0 };
    int ret;

    newdata.ctx = data->ctx;
    newdata.mem_fd = buf_fd;
    newdata.size = data->size;
    newdata.mali_address = data->mali_address;
    newdata.rights = data->rights;
    newdata.flags = data->flags;

    ret = hook.ioctl(hook.mali_fd, MALI_IOC_MEM_ATTACH_DMA_BUF, &newdata);

    data->cookie = newdata.cookie;
    return ret;
  } else {
    return -ENOTTY;
  }
}

static int emulate_mali_mem_unmap_ext(void *ptr) {
#ifdef HOOK_VERBOSE
  fprintf(stderr, "info: emulate_mali_mem_unmap_ext called\n");
  fprintf(stderr, "info: translating to dma-buf release\n");
#endif

  /* data structures are compatible */
  return hook.ioctl(hook.mali_fd, MALI_IOC_MEM_RELEASE_DMA_BUF, ptr);
}

int open(const char *pathname, int flags, mode_t mode) {
  int fd;

  if (hook.open == NULL)
    hook.open = (openfnc)dlsym(RTLD_NEXT, "open");

  if (strcmp(pathname, fbdev_name) == 0) {
    fprintf(stderr, "open called (fbdev)\n");
    hook.fbdev_fd = hook.open(fake_fbdev, O_RDWR, 0);
#ifdef HOOK_VERBOSE
    fprintf(stderr, "fake fbdev fd = %d\n", hook.fbdev_fd);
#endif

    if (hinit && hinit(&hook)) {
      fprintf(stderr, "error: hook initialization failed\n");
      hook.close(hook.fbdev_fd);
      hook.fbdev_fd = -1;
      return -1;
    }

    fd = hook.fbdev_fd;
  } else if (strcmp(pathname, mali_name) == 0) {
    fprintf(stderr, "open called (mali)\n");
    hook.mali_fd = hook.open(pathname, flags, mode);
#ifdef HOOK_VERBOSE
    fprintf(stderr, "mali fd = %d\n", hook.mali_fd);
#endif
    fd = hook.mali_fd;
  } else {
    fd = hook.open(pathname, flags, mode);
  }

  return fd;
}

int close(int fd) {
  if (hook.close == NULL)
    hook.close = (closefnc)dlsym(RTLD_NEXT, "close");

  if (fd == hook.fbdev_fd) {
    fprintf(stderr, "close called on fake fbdev fd\n");

    if (hfree && hfree(&hook)) {
      fprintf(stderr, "error: freeing hook failed\n");
      return -1;
    }

    hook.fbdev_fd = -1;
  } else if (fd == hook.mali_fd) {
    fprintf(stderr, "close called on mali fd\n");
    hook.mali_fd = -1;
  }

  return hook.close(fd);
}

void *mmap(void *addr, size_t length, int prot,
           int flags, int fd, off_t offset) {
  if (hook.mmap == NULL)
    hook.mmap = (mmapfnc)dlsym(RTLD_NEXT, "mmap");

  if (hook.fbdev_fd != -1 && fd == hook.fbdev_fd) {
    fprintf(stderr, "mmap called on fake fbdev fd\n");

    void *ret = NULL;

    if (prot == PROT_WRITE && flags == MAP_SHARED) {
      if (hook.fake_mmap == NULL)
        hook.fake_mmap = malloc(length);

      ret = hook.fake_mmap;
    }

    return ret;
  } else {
    /* pass-through */
    return hook.mmap(addr, length, prot, flags, fd, offset);
  }
}

int munmap(void *addr, size_t length) {
  if (hook.munmap == NULL)
    hook.munmap = (munmapfnc)dlsym(RTLD_NEXT, "munmap");

  if (hook.fake_mmap != NULL && addr == hook.fake_mmap) {
    fprintf(stderr, "munmap called on fake fbdev fd\n");

    free(hook.fake_mmap);
    hook.fake_mmap = NULL;

    return 0;
  } else {
    /* pass-through */
    return hook.munmap(addr, length);
  }
}

int ioctl(int fd, unsigned long request, ...) {
  int ret = -1;

  if (hook.ioctl == NULL)
    hook.ioctl = (ioctlfnc)dlsym(RTLD_NEXT, "ioctl");

  va_list args;

  va_start(args, request);
  void *p = va_arg(args, void *);
  va_end(args);

  if (fd == hook.fbdev_fd) {
    switch (request) {
      case FBIOGET_VSCREENINFO:
        ret = emulate_get_var_screeninfo(p);
        break;

      case FBIOPUT_VSCREENINFO:
        ret = emulate_put_var_screeninfo(p);
        break;

      case FBIOGET_FSCREENINFO:
        ret = emulate_get_fix_screeninfo(p);
        break;

      case FBIOPAN_DISPLAY:
        ret = emulate_pan_display(p);
        break;

      case FBIO_WAITFORVSYNC:
        ret = emulate_waitforvsync(p);
        break;

      case IOCTL_GET_FB_DMA_BUF:
        ret = emulate_get_fb_dma_buf(p);
        break;

      default:
        fprintf(stderr, "info: unhooked fbdev ioctl (0x%x) called\n", (unsigned int)request);
        ret = hook.ioctl(fd, request, p);
        break;
    }
  } else if (fd == hook.mali_fd) {
    switch (request) {
      case MALI_IOC_MEM_MAP_EXT:
        ret = emulate_mali_mem_map_ext(p);
        break;

      case MALI_IOC_MEM_UNMAP_EXT:
        ret = emulate_mali_mem_unmap_ext(p);
        break;

      default:
#ifdef HOOK_VERBOSE
        fprintf(stderr, "info: unhooked mali ioctl (%s) called\n", translate_mali_ioctl(request));
#endif
        ret = hook.ioctl(fd, request, p);
        break;
    }
  } else {
    /* pass-through */
    ret = hook.ioctl(fd, request, p);
  }

  return ret;
}
