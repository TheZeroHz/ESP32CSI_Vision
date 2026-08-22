#include "yolo26.hpp"
#if __has_include("dl_image_jpeg.hpp")
#include "dl_image_jpeg.hpp"
#endif
#include "dl_math.hpp"
#include "dl_tool.hpp"
#include "esp_heap_caps.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>

// --- Constructor ---

YOLO26::YOLO26(dl::Model *model, int k, float thresh, const char **classes) :
    target_k(k), conf_thresh(thresh), class_names(classes)
{
    // Reserve memory for grids
    grid_sizes.resize(3);

    // Initialize the preprocessor with standard YOLO Mean (0) and Std (255)
    m_image_preprocessor = new dl::image::ImagePreprocessor(model, {0, 0, 0}, {255, 255, 255});
    m_image_preprocessor->enable_letterbox({114, 114, 114});

    // Calculate grid sizes using model inputs
    auto inputs = model->get_inputs();
    if (!inputs.empty() && inputs.begin()->second) {
        dl::TensorBase *input_tensor = inputs.begin()->second;
        int input_w = input_tensor->shape[2];
        for (int i = 0; i < 3; i++) {
            grid_sizes[i] = input_w / strides[i];
        }
    }
}

YOLO26::~YOLO26()
{
    delete m_image_preprocessor;
}

// --- Methods ---

dl::image::img_t YOLO26::decode_jpeg(const uint8_t *jpg_data, size_t jpg_len)
{
#if __has_include("dl_image_jpeg.hpp")
    dl::image::jpeg_img_t jpeg_img = {.data = (void *)jpg_data, .data_len = jpg_len};
    return dl::image::sw_decode_jpeg(jpeg_img, dl::image::DL_IMAGE_PIX_TYPE_RGB888);
#else
    (void)jpg_data;
    (void)jpg_len;
    return {};
#endif
}

void YOLO26::preprocess(const dl::image::img_t &img)
{
    m_image_preprocessor->preprocess(img);
}

template <typename T>
void YOLO26::decode_grid(dl::TensorBase *p_box,
                         dl::TensorBase *p_cls,
                         int stride,
                         int grid_h,
                         int grid_w,
                         bool nhwc,
                         std::vector<dl::detect::result_t> &candidates)
{
    float box_scale = DL_SCALE(p_box->exponent);
    float cls_scale = DL_SCALE(p_cls->exponent);
    T *raw_box = p_box->get_element_ptr<T>();
    T *raw_cls = p_cls->get_element_ptr<T>();
    if (!raw_box || !raw_cls) return;
    const int hw = grid_h * grid_w;

    float raw_thresh_float = dl::math::inverse_sigmoid(conf_thresh);
    double t = std::floor((double)raw_thresh_float / (double)cls_scale);
    const double tmax = (double)std::numeric_limits<T>::max();
    const double tmin = (double)std::numeric_limits<T>::min();
    if (t > tmax) t = tmax;
    if (t < tmin) t = tmin;
    T cls_thresh = (T)t;

    for (int h = 0; h < grid_h; h++) {
        for (int w = 0; w < grid_w; w++) {
            int pixel_idx = (h * grid_w) + w;
            float max_score = -1.0f;
            int best_cls_id = -1;

            for (int c = 0; c < num_classes; c++) {
                T raw_val_T = nhwc ? raw_cls[pixel_idx * num_classes + c] : raw_cls[c * hw + pixel_idx];
                if (raw_val_T <= cls_thresh) continue;

                float score = dl::math::sigmoid(dl::dequantize(raw_val_T, cls_scale));
                if (score > max_score) {
                    max_score = score;
                    best_cls_id = c;
                }
            }

            if (best_cls_id < 0 || max_score < conf_thresh) continue;

            float d_l, d_t, d_r, d_b;
            if (nhwc) {
                int box_offset = pixel_idx * 4;
                d_l = dl::dequantize(raw_box[box_offset + 0], box_scale);
                d_t = dl::dequantize(raw_box[box_offset + 1], box_scale);
                d_r = dl::dequantize(raw_box[box_offset + 2], box_scale);
                d_b = dl::dequantize(raw_box[box_offset + 3], box_scale);
            } else {
                d_l = dl::dequantize(raw_box[0 * hw + pixel_idx], box_scale);
                d_t = dl::dequantize(raw_box[1 * hw + pixel_idx], box_scale);
                d_r = dl::dequantize(raw_box[2 * hw + pixel_idx], box_scale);
                d_b = dl::dequantize(raw_box[3 * hw + pixel_idx], box_scale);
            }

            float cx = w + 0.5f;
            float cy = h + 0.5f;
            float x1 = (cx - d_l) * stride;
            float y1 = (cy - d_t) * stride;
            float x2 = (cx + d_r) * stride;
            float y2 = (cy + d_b) * stride;

            candidates.push_back({best_cls_id, max_score, {(int)x1, (int)y1, (int)x2, (int)y2}, {}});
        }
    }
}

std::vector<dl::detect::result_t> YOLO26::postprocess(const std::map<std::string, dl::TensorBase *> &outputs)
{
    if (grid_sizes.empty() || grid_sizes[0] == 0) {
        printf("[YOLO26] Error: Grid sizes not initialized. Call preprocess() first.\n");
        return {};
    }

    auto pick = [&](const char *name) -> dl::TensorBase * {
        auto it = outputs.find(name);
        return it == outputs.end() ? nullptr : it->second;
    };
    dl::TensorBase *p3_box = pick("one2one_p3_box");
    dl::TensorBase *p4_box = pick("one2one_p4_box");
    dl::TensorBase *p5_box = pick("one2one_p5_box");
    dl::TensorBase *p3_cls = pick("one2one_p3_cls");
    dl::TensorBase *p4_cls = pick("one2one_p4_cls");
    dl::TensorBase *p5_cls = pick("one2one_p5_cls");
    if (!p3_box || !p4_box || !p5_box || !p3_cls || !p4_cls || !p5_cls) {
        static bool once = false;
        if (!once) {
            once = true;
            printf("[YOLO26] missing outputs. have:");
            for (const auto &kv : outputs) printf(" %s", kv.first.c_str());
            printf("\n");
        }
        return {};
    }

    this->num_classes = p3_cls->shape[3];
    const bool nhwc = (p3_cls->shape.size() >= 4 && p5_cls->shape.size() >= 4 &&
                       p3_cls->shape[3] == p5_cls->shape[3] && p3_cls->shape[3] >= 4);
    if (!nhwc && p3_cls->shape.size() >= 4) this->num_classes = p3_cls->shape[1];

    static bool dumped = false;
    if (!dumped) {
        dumped = true;
        printf("[YOLO26] layout=%s classes=%d thresh=%.2f\n", nhwc ? "NHWC" : "NCHW", this->num_classes,
               conf_thresh);
        auto dump = [](const char *name, dl::TensorBase *t) {
            printf("[YOLO26] %s dtype=%d exp=%d shape", name, (int)t->dtype, (int)t->exponent);
            for (int s : t->shape) printf(" %d", s);
            printf("\n");
        };
        dump("p3_box", p3_box);
        dump("p3_cls", p3_cls);
        dump("p4_box", p4_box);
        dump("p4_cls", p4_cls);
        dump("p5_box", p5_box);
        dump("p5_cls", p5_cls);
        const float inv_x = m_image_preprocessor ? m_image_preprocessor->get_resize_scale_x(true) : 0;
        const float inv_y = m_image_preprocessor ? m_image_preprocessor->get_resize_scale_y(true) : 0;
        printf("[YOLO26] letterbox inv=%.3f,%.3f pad=%d,%d\n", inv_x, inv_y,
               m_image_preprocessor ? m_image_preprocessor->get_border_left() : 0,
               m_image_preprocessor ? m_image_preprocessor->get_border_top() : 0);
        fflush(stdout);
    }

    std::vector<dl::detect::result_t> candidates;
    candidates.reserve(target_k * 2);
    unsigned lvl_n[3] = {0, 0, 0};
    dl::TensorBase *boxes[] = {p3_box, p4_box, p5_box};
    dl::TensorBase *clss[] = {p3_cls, p4_cls, p5_cls};

    for (int i = 0; i < 3; i++) {
        int stride = strides[i];
        int grid_h = nhwc ? clss[i]->shape[1] : clss[i]->shape[2];
        int grid_w = nhwc ? clss[i]->shape[2] : clss[i]->shape[3];
        if (grid_h <= 0 || grid_w <= 0) {
            grid_h = grid_sizes[i];
            grid_w = grid_sizes[i];
        }

        const size_t before = candidates.size();
        if (boxes[i]->dtype == dl::DATA_TYPE_INT8 && clss[i]->dtype == dl::DATA_TYPE_INT8) {
            decode_grid<int8_t>(boxes[i], clss[i], stride, grid_h, grid_w, nhwc, candidates);
        } else if (boxes[i]->dtype == dl::DATA_TYPE_INT16 && clss[i]->dtype == dl::DATA_TYPE_INT16) {
            decode_grid<int16_t>(boxes[i], clss[i], stride, grid_h, grid_w, nhwc, candidates);
        } else {
            printf("[YOLO26] Error: Unsupported tensor dtype box=%d cls=%d\n", (int)boxes[i]->dtype,
                   (int)clss[i]->dtype);
            return {};
        }
        const unsigned got = (unsigned)(candidates.size() - before);
        const unsigned cells = (unsigned)(grid_h * grid_w);
        lvl_n[i] = got;
        /* Guard: a head that fires on most cells is noise (bad AE / cache), not COCO. */
        if (cells > 0 && got * 4u > cells * 3u) {
            candidates.resize(before);
            lvl_n[i] = 0;
            printf("[YOLO26] drop p%d saturated %u/%u\n", 3 + i, got, cells);
        }
    }

    static int nprint = 0;
    if (nprint < 3) {
        nprint++;
        float max_s = 0.f;
        int raw0 = 0, raw1 = 0, tsize = p3_cls->get_size();
        if (p3_cls->dtype == dl::DATA_TYPE_INT16 && p3_cls->data) {
            const int16_t *c = (const int16_t *)p3_cls->data;
            raw0 = c[0];
            raw1 = c[80];
            const float sc = DL_SCALE(p3_cls->exponent);
            int nn = 1;
            for (int s : p3_cls->shape) nn *= (s > 0) ? s : 1;
            for (int i = 0; i < nn; i += 19) {
                float s = dl::math::sigmoid(dl::dequantize(c[i], sc));
                if (s > max_s) max_s = s;
            }
        }
        printf("[YOLO26] cand=%u p3=%u p4=%u p5=%u max=%.3f size=%d raw=%d,%d\n",
               (unsigned)candidates.size(), lvl_n[0], lvl_n[1], lvl_n[2], max_s, tsize, raw0, raw1);
        fflush(stdout);
    }

    if (candidates.size() > (size_t)target_k) {
        std::nth_element(candidates.begin(), candidates.begin() + target_k, candidates.end(),
                         dl::detect::greater_box);
        candidates.resize(target_k);
    }

    /* Boxes are in letterboxed model input pixels — map back to preprocess source. */
    if (m_image_preprocessor) {
        const float inv_x = m_image_preprocessor->get_resize_scale_x(true);
        const float inv_y = m_image_preprocessor->get_resize_scale_y(true);
        const int left = m_image_preprocessor->get_border_left();
        const int top = m_image_preprocessor->get_border_top();
        for (auto &r : candidates) {
            if (r.box.size() < 4) continue;
            r.box[0] = (int)(((float)r.box[0] - (float)left) * inv_x);
            r.box[1] = (int)(((float)r.box[1] - (float)top) * inv_y);
            r.box[2] = (int)(((float)r.box[2] - (float)left) * inv_x);
            r.box[3] = (int)(((float)r.box[3] - (float)top) * inv_y);
        }
    }

    return candidates;
}

