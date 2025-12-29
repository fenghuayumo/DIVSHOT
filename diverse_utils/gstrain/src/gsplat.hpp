#ifndef GSPLAT_H
#define GSPLAT_H

#include <rasterizer/gsplat-cuda/config.h>

#if defined(USE_HIP) || defined(USE_CUDA)
#include <rasterizer/gsplat-cuda/ops.h>
#endif

#if defined(USE_MPS)
#include <rasterizer/gsplat-metal/ops.h>
#endif
#include <torch/all.h>
#endif
