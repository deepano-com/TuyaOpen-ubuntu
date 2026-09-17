/**
 * @file tkl_npu.c
 * @brief NPU inference: hbDNN glue + YOLOv5 decode/NMS (pure C)
 * @version 0.1
 * @date 2026-09-17
 *
 * @copyright Copyright 2026 Tuya Inc. All Rights Reserved.
 *
 * Pipeline (matches the validated python reference):
 *   NV12(model size) -> hbDNNInfer -> 3x F32 NHWC -> decode ->
 *   score filter -> class-aware NMS (model space) -> scale to ori size.
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "hb_dnn.h"
#include "hb_sys.h"

#include "tkl_npu.h"

#define NPU_TASK_TIMEOUT_MS 5000
#define NPU_NUM_ANCHOR      3
#define NPU_NUM_PRED        85 /* 4 box + 1 obj + 80 cls */
#define NPU_NUM_CLASS       80
#define NPU_MAX_CAND        30000
#define NPU_MAX_OUT         8

#define NPU_DEF_SCORE_THRES 0.25f
#define NPU_DEF_NMS_THRES   0.45f

static const char *npu_coco_names[NPU_NUM_CLASS] = {
    "person",        "bicycle",    "car",          "motorcycle",   "airplane",     "bus",
    "train",         "truck",      "boat",         "traffic light", "fire hydrant", "stop sign",
    "parking meter", "bench",      "bird",         "cat",          "dog",          "horse",
    "sheep",         "cow",        "elephant",     "bear",         "zebra",        "giraffe",
    "backpack",      "umbrella",   "handbag",      "tie",          "suitcase",     "frisbee",
    "skis",          "snowboard",  "sports ball",  "kite",         "baseball bat", "baseball glove",
    "skateboard",    "surfboard",  "tennis racket", "bottle",      "wine glass",   "cup",
    "fork",          "knife",      "spoon",        "bowl",         "banana",       "apple",
    "sandwich",      "orange",     "broccoli",     "carrot",       "hot dog",      "pizza",
    "donut",         "cake",       "chair",        "couch",        "potted plant", "bed",
    "dining table",  "toilet",     "tv",           "laptop",       "mouse",        "remote",
    "keyboard",      "cell phone", "microwave",    "oven",         "toaster",      "sink",
    "refrigerator",  "book",       "clock",        "vase",         "scissors",     "teddy bear",
    "hair drier",    "toothbrush",
};

/* anchors[layer][anchor][x/y], layer0 = stride8 (largest feature map) */
static const float npu_anchors[3][NPU_NUM_ANCHOR][2] = {
    {{10, 13}, {16, 30}, {33, 23}},
    {{30, 61}, {62, 45}, {59, 119}},
    {{116, 90}, {156, 198}, {373, 326}},
};

/* decode candidate in MODEL coordinates */
typedef struct {
    float x1, y1, x2, y2;
    int   id;
    float score;
    float area;
    int   idx; /* decode order, for stable sort */
} npu_cand_t;

typedef struct {
    hbPackedDNNHandle_t packed;
    hbDNNHandle_t       dnn;
    hbDNNTensor         input;  /* pre-allocated once, reused */
    hbDNNTensor        *output; /* pre-allocated once, reused */
    uint32_t            in_bytes;
    int                 model_w, model_h;
    int                 out_count;
    float               score_thres;
    float               nms_thres;
} npu_handle_t;

static float npu_sigmoid(float x)
{
    return 1.0f / (1.0f + expf(-x));
}

OPERATE_RET tkl_npu_load(const char *model_path, TKL_NPU_HANDLE *handle)
{
    if (NULL == model_path || NULL == handle) {
        return OPRT_INVALID_PARM;
    }
    *handle = NULL;

    npu_handle_t         *h     = NULL;
    const char           *files[] = {model_path};
    char const          **names  = NULL;
    int32_t               ncount = 0;
    int32_t               icount = 0;
    hbDNNTensorProperties iprops;

    h = (npu_handle_t *)calloc(1, sizeof(npu_handle_t));
    if (NULL == h) {
        return OPRT_MALLOC_FAILED;
    }
    h->score_thres = NPU_DEF_SCORE_THRES;
    h->nms_thres   = NPU_DEF_NMS_THRES;

    if (0 != hbDNNInitializeFromFiles(&h->packed, files, 1) || NULL == h->packed) {
        goto fail;
    }

    if (0 != hbDNNGetModelNameList(&names, &ncount, h->packed) || ncount < 1) {
        goto fail;
    }
    if (0 != hbDNNGetModelHandle(&h->dnn, h->packed, names[0]) || NULL == h->dnn) {
        goto fail;
    }

    /* v1: exactly 1 NV12 input */
    hbDNNGetInputCount(&icount, h->dnn);
    if (1 != icount) {
        goto fail;
    }
    memset(&iprops, 0, sizeof(iprops));
    hbDNNGetInputTensorProperties(&iprops, h->dnn, 0);
    if (HB_DNN_IMG_TYPE_NV12 != iprops.tensorType) {
        goto fail;
    }
    /* input valid shape is pseudo-NCHW: [N, C, H, W] */
    h->model_h = iprops.validShape.dimensionSize[2];
    h->model_w = iprops.validShape.dimensionSize[3];
    h->in_bytes = (uint32_t)iprops.alignedByteSize;
    if (h->model_w <= 0 || h->model_h <= 0 ||
        (uint32_t)(h->model_w * h->model_h * 3 / 2) != h->in_bytes) {
        goto fail;
    }

    memset(&h->input, 0, sizeof(h->input));
    if (0 != hbSysAllocCachedMem(&h->input.sysMem[0], h->in_bytes)) {
        goto fail;
    }
    h->input.properties = iprops;

    /* v1: F32 NHWC outputs (yolov5s_672x672_nv12: 3 outputs) */
    hbDNNGetOutputCount(&h->out_count, h->dnn);
    if (h->out_count <= 0 || h->out_count > NPU_MAX_OUT) {
        goto fail;
    }
    for (int i = 0; i < h->out_count; i++) {
        hbDNNTensorProperties oprops;
        memset(&oprops, 0, sizeof(oprops));
        hbDNNGetOutputTensorProperties(&oprops, h->dnn, i);
        if (HB_DNN_LAYOUT_NHWC != oprops.tensorLayout ||
            HB_DNN_TENSOR_TYPE_F32 != oprops.tensorType) {
            goto fail;
        }
    }

    /* runtime does not alloc outputs: pre-allocate once, reuse per infer */
    h->output = (hbDNNTensor *)calloc((size_t)h->out_count, sizeof(hbDNNTensor));
    if (NULL == h->output) {
        goto fail;
    }
    for (int i = 0; i < h->out_count; i++) {
        hbDNNTensorProperties oprops;
        memset(&oprops, 0, sizeof(oprops));
        hbDNNGetOutputTensorProperties(&oprops, h->dnn, i);
        if (0 != hbSysAllocCachedMem(&h->output[i].sysMem[0], (uint32_t)oprops.alignedByteSize)) {
            goto fail;
        }
        h->output[i].properties = oprops;
    }

    *handle = (TKL_NPU_HANDLE)h;
    return OPRT_OK;

fail:
    tkl_npu_unload((TKL_NPU_HANDLE)h);
    return OPRT_COM_ERROR;
}

OPERATE_RET tkl_npu_unload(TKL_NPU_HANDLE handle)
{
    npu_handle_t *h = (npu_handle_t *)handle;
    if (NULL == h) {
        return OPRT_OK;
    }
    if (h->input.sysMem[0].virAddr) {
        hbSysFreeMem(&h->input.sysMem[0]);
    }
    if (h->output) {
        for (int i = 0; i < h->out_count; i++) {
            if (h->output[i].sysMem[0].virAddr) {
                hbSysFreeMem(&h->output[i].sysMem[0]);
            }
        }
        free(h->output);
    }
    if (h->packed) {
        hbDNNRelease(h->packed);
    }
    free(h);
    return OPRT_OK;
}

OPERATE_RET tkl_npu_get_input_spec(TKL_NPU_HANDLE handle, int *w, int *h)
{
    npu_handle_t *hd = (npu_handle_t *)handle;
    if (NULL == hd || NULL == w || NULL == h) {
        return OPRT_INVALID_PARM;
    }
    *w = hd->model_w;
    *h = hd->model_h;
    return OPRT_OK;
}

OPERATE_RET tkl_npu_set_thresh(TKL_NPU_HANDLE handle, float score_thres, float nms_thres)
{
    npu_handle_t *hd = (npu_handle_t *)handle;
    if (NULL == hd) {
        return OPRT_INVALID_PARM;
    }
    hd->score_thres = score_thres;
    hd->nms_thres   = nms_thres;
    return OPRT_OK;
}

/* decode one output layer (NHWC float) into candidates, model coords */
static int npu_decode_layer(hbDNNTensor *out, int stride, const float anchors[NPU_NUM_ANCHOR][2],
                            float score_thres, npu_cand_t *cands, int cap)
{
    hbSysFlushMem(&out->sysMem[0], HB_SYS_MEM_CACHE_INVALIDATE);

    int H = out->properties.validShape.dimensionSize[1];
    int W = out->properties.validShape.dimensionSize[2];
    int C = out->properties.validShape.dimensionSize[3];
    if (H <= 0 || W <= 0 || C != NPU_NUM_ANCHOR * NPU_NUM_PRED) {
        return -1;
    }

    float *data = (float *)out->sysMem[0].virAddr;
    if (NULL == data) {
        return -1;
    }

    int n = 0;
    for (int h = 0; h < H; h++) {
        for (int w = 0; w < W; w++) {
            for (int k = 0; k < NPU_NUM_ANCHOR; k++) {
                float *d   = data + ((h * W + w) * NPU_NUM_ANCHOR + k) * NPU_NUM_PRED;
                float  obj = npu_sigmoid(d[4]);

                int   id     = 0;
                float clsmax = d[5];
                for (int c = 1; c < NPU_NUM_CLASS; c++) {
                    if (d[5 + c] > clsmax) {
                        clsmax = d[5 + c];
                        id     = c;
                    }
                }
                float conf = obj * npu_sigmoid(clsmax);
                if (conf < score_thres) {
                    continue;
                }

                float anchor_x = anchors[k][0];
                float anchor_y = anchors[k][1];
                float cx = (npu_sigmoid(d[0]) * 2.0f - 0.5f + (float)w) * (float)stride;
                float cy = (npu_sigmoid(d[1]) * 2.0f - 0.5f + (float)h) * (float)stride;
                float sx = npu_sigmoid(d[2]);
                float sy = npu_sigmoid(d[3]);
                sx       = (sx * 2.0f) * (sx * 2.0f) * anchor_x;
                sy       = (sy * 2.0f) * (sy * 2.0f) * anchor_y;

                float x1 = cx - sx / 2.0f;
                float y1 = cy - sy / 2.0f;
                float x2 = cx + sx / 2.0f;
                float y2 = cy + sy / 2.0f;
                if (x2 <= 0 || y2 <= 0 || x1 > x2 || y1 > y2) {
                    continue;
                }
                if (n >= cap) {
                    return n;
                }
                cands[n].x1    = x1;
                cands[n].y1    = y1;
                cands[n].x2    = x2;
                cands[n].y2    = y2;
                cands[n].id    = id;
                cands[n].score = conf;
                cands[n].area  = (x2 - x1) * (y2 - y1);
                cands[n].idx   = n;
                n++;
            }
        }
    }
    return n;
}

static int npu_cand_cmp(const void *a, const void *b)
{
    const npu_cand_t *ca = (const npu_cand_t *)a;
    const npu_cand_t *cb = (const npu_cand_t *)b;
    if (ca->score != cb->score) {
        return (ca->score > cb->score) ? -1 : 1; /* desc */
    }
    return ca->idx - cb->idx; /* stable: decode order */
}

/* greedy class-aware NMS over model-space boxes; returns survivors (in place) */
static int npu_nms(npu_cand_t *cands, int n, float iou_thres)
{
    if (n <= 0) {
        return 0;
    }
    qsort(cands, (size_t)n, sizeof(npu_cand_t), npu_cand_cmp);

    int kept = 0;
    for (int i = 0; i < n; i++) {
        if (cands[i].score < 0) { /* suppressed marker */
            continue;
        }
        /* move survivor to front */
        if (kept != i) {
            cands[kept] = cands[i];
        }
        for (int j = i + 1; j < n; j++) {
            if (cands[j].score < 0 || cands[j].id != cands[kept].id) {
                continue;
            }
            float xx1 = cands[kept].x1 > cands[j].x1 ? cands[kept].x1 : cands[j].x1;
            float yy1 = cands[kept].y1 > cands[j].y1 ? cands[kept].y1 : cands[j].y1;
            float xx2 = cands[kept].x2 < cands[j].x2 ? cands[kept].x2 : cands[j].x2;
            float yy2 = cands[kept].y2 < cands[j].y2 ? cands[kept].y2 : cands[j].y2;
            if (xx2 > xx1 && yy2 > yy1) {
                float inter = (xx2 - xx1) * (yy2 - yy1);
                float iou   = inter / (cands[kept].area + cands[j].area - inter);
                if (iou > iou_thres) {
                    cands[j].score = -1.0f;
                }
            }
        }
        kept++;
    }
    return kept;
}

OPERATE_RET tkl_npu_detect(TKL_NPU_HANDLE handle, const TKL_NPU_IMG_T *img, TKL_NPU_OBJ_T *objs,
                           int *obj_num)
{
    npu_handle_t *h = (npu_handle_t *)handle;
    if (NULL == h || NULL == img || NULL == img->nv12 || NULL == objs || NULL == obj_num ||
        *obj_num <= 0) {
        return OPRT_INVALID_PARM;
    }
    if (img->model_w != h->model_w || img->model_h != h->model_h || img->ori_w <= 0 ||
        img->ori_h <= 0) {
        return OPRT_INVALID_PARM;
    }
    int cap = *obj_num; /* save capacity before clearing */
    *obj_num = 0;

    memcpy(h->input.sysMem[0].virAddr, img->nv12, h->in_bytes);
    hbSysFlushMem(&h->input.sysMem[0], HB_SYS_MEM_CACHE_CLEAN);

    hbDNNInferCtrlParam ctrl;
    memset(&ctrl, 0, sizeof(ctrl));

    hbDNNTaskHandle_t task   = NULL;
    hbDNNTensor      *output = h->output;
    if (0 != hbDNNInfer(&task, &output, &h->input, h->dnn, &ctrl) || NULL == task) {
        return OPRT_COM_ERROR;
    }
    if (0 != hbDNNWaitTaskDone(task, NPU_TASK_TIMEOUT_MS)) {
        hbDNNReleaseTask(task);
        return OPRT_TIMEOUT;
    }

    npu_cand_t *cands = (npu_cand_t *)malloc(sizeof(npu_cand_t) * NPU_MAX_CAND);
    if (NULL == cands) {
        hbDNNReleaseTask(task);
        return OPRT_MALLOC_FAILED;
    }

    /* order outputs by H desc -> layer(stride 8/16/32), robust to any order */
    int order[NPU_MAX_OUT];
    for (int i = 0; i < h->out_count; i++) {
        order[i] = i;
    }
    for (int i = 0; i < h->out_count; i++) {
        for (int j = i + 1; j < h->out_count; j++) {
            int hi = output[order[i]].properties.validShape.dimensionSize[1];
            int hj = output[order[j]].properties.validShape.dimensionSize[1];
            if (hj > hi) {
                int t = order[i];
                order[i] = order[j];
                order[j] = t;
            }
        }
    }

    int ncand = 0;
    for (int l = 0; l < h->out_count && l < 3; l++) {
        hbDNNTensor *out = &output[order[l]];
        int          W    = out->properties.validShape.dimensionSize[2];
        if (W <= 0) {
            continue;
        }
        int stride = h->model_w / W;
        int n = npu_decode_layer(out, stride, npu_anchors[l], h->score_thres, cands + ncand,
                                 NPU_MAX_CAND - ncand);
        if (n > 0) {
            ncand += n;
        }
    }

    int kept = npu_nms(cands, ncand, h->nms_thres);
    float    sx   = (float)img->ori_w / (float)h->model_w;
    float    sy   = (float)img->ori_h / (float)h->model_h;
    int      ncpy = kept < cap ? kept : cap;
    for (int i = 0; i < ncpy; i++) {
        float x1 = cands[i].x1 * sx;
        float y1 = cands[i].y1 * sy;
        float x2 = cands[i].x2 * sx;
        float y2 = cands[i].y2 * sy;
        if (x1 < 0) x1 = 0;
        if (y1 < 0) y1 = 0;
        if (x2 > (float)(img->ori_w - 1)) x2 = (float)(img->ori_w - 1);
        if (y2 > (float)(img->ori_h - 1)) y2 = (float)(img->ori_h - 1);
        objs[i].x1       = x1;
        objs[i].y1       = y1;
        objs[i].x2       = x2;
        objs[i].y2       = y2;
        objs[i].class_id = cands[i].id;
        objs[i].score    = cands[i].score;
        strncpy(objs[i].label, npu_coco_names[cands[i].id], TKL_NPU_LABEL_LEN - 1);
        objs[i].label[TKL_NPU_LABEL_LEN - 1] = '\0';
    }
    *obj_num = ncpy;

    free(cands);
    hbDNNReleaseTask(task);
    return OPRT_OK;
}
