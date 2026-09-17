/**
 * @file tkl_npu.h
 * @brief NPU inference module (D-Robotics X5 BPU, YOLOv5 / COCO)
 * @version 0.1
 * @date 2026-09-17
 *
 * @copyright Copyright 2026 Tuya Inc. All Rights Reserved.
 *
 * @note v1 scope: single YOLOv5 model with F32 outputs (e.g.
 *       yolov5s_672x672_nv12.bin), NV12 input at model resolution,
 *       COCO-80 labels, one thread per handle.
 */
#ifndef __TKL_NPU_H__
#define __TKL_NPU_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TKL_NPU_LABEL_LEN 32
#define TKL_NPU_MAX_OBJ   128

/** NPU model handle (supports multiple loaded models) */
typedef void *TKL_NPU_HANDLE;

/** One detected object in ORIGINAL image coordinates */
typedef struct {
    float x1, y1, x2, y2;
    int   class_id;
    char  label[TKL_NPU_LABEL_LEN];
    float score;
} TKL_NPU_OBJ_T;

/** Input image descriptor */
typedef struct {
    const uint8_t *nv12;  /**< contiguous NV12 blob (Y plane + UV plane) */
    int            model_w; /**< must equal the loaded model input size */
    int            model_h; /**< (query with tkl_npu_get_input_spec) */
    int            ori_w;   /**< original image size, for box mapping */
    int            ori_h;
} TKL_NPU_IMG_T;

/**
 * @brief Load a .bin model onto the BPU, query its input spec
 *
 * @param[in] model_path path to the .bin model file
 * @param[out] handle loaded model handle
 * @return OPRT_OK on success
 */
OPERATE_RET tkl_npu_load(const char *model_path, TKL_NPU_HANDLE *handle);

/**
 * @brief Get model input size (caller resizes + converts to NV12)
 */
OPERATE_RET tkl_npu_get_input_spec(TKL_NPU_HANDLE handle, int *w, int *h);

/**
 * @brief Set post-process thresholds (default: score 0.25, nms 0.45)
 */
OPERATE_RET tkl_npu_set_thresh(TKL_NPU_HANDLE handle, float score_thres, float nms_thres);

/**
 * @brief Run one synchronous detection
 *
 * @param[in] handle model handle from tkl_npu_load
 * @param[in] img input image (model-size NV12 + original size)
 * @param[out] objs caller array for results (TKL_NPU_MAX_OBJ recommended)
 * @param[in,out] obj_num in: objs capacity; out: objects copied
 * @return OPRT_OK on success
 */
OPERATE_RET tkl_npu_detect(TKL_NPU_HANDLE handle, const TKL_NPU_IMG_T *img, TKL_NPU_OBJ_T *objs,
                           int *obj_num);

/**
 * @brief Unload model and free all resources
 */
OPERATE_RET tkl_npu_unload(TKL_NPU_HANDLE handle);

#ifdef __cplusplus
}
#endif

#endif /* __TKL_NPU_H__ */
