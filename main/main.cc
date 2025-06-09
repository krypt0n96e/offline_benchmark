#include <stdio.h>
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

// --- Thư viện cần thiết cho PSRAM ---
#include "esp_heap_caps.h"
// --- Thư viện cần thiết cho Watchdog Timer và FreeRTOS ---
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// --- Thư viện TensorFlow Lite Micro ---
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

// --- Dữ liệu model và test (được include từ các file riêng) ---
#include "model.cc"
#include "test_data.cc"

#define TAG "OfflineBenchmark"

// ==============================================================================
// Toàn bộ logic benchmark được chuyển vào một tác vụ riêng
// ==============================================================================
static void benchmark_task(void *pvParameter)
{
    ESP_LOGI(TAG, "Benchmark task started on CPU %d", xPortGetCoreID());

    // --- Thiết lập TensorFlow Lite Micro ---
    const tflite::Model* model_data = tflite::GetModel(model); 
    if (model_data->version() != TFLITE_SCHEMA_VERSION) {
        ESP_LOGE(TAG, "Model schema version mismatch!");
        vTaskDelete(NULL); // Xóa tác vụ nếu có lỗi
        return;
    }

    tflite::MicroMutableOpResolver<12> micro_resolver;
    micro_resolver.AddConv2D();
    micro_resolver.AddMaxPool2D();
    micro_resolver.AddRelu();
    micro_resolver.AddFullyConnected();
    micro_resolver.AddReshape();
    micro_resolver.AddSoftmax();
    micro_resolver.AddPad();
    micro_resolver.AddTranspose();
    micro_resolver.AddDequantize();


    const int kTensorArenaSize = 700 * 1024; // 700 KB
    
    uint8_t* tensor_arena = (uint8_t*) heap_caps_malloc(kTensorArenaSize, MALLOC_CAP_SPIRAM);
    if (tensor_arena == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for tensor_arena in PSRAM!");
        vTaskDelete(NULL);
        return;
    }

    tflite::MicroInterpreter interpreter(model_data, micro_resolver, tensor_arena, kTensorArenaSize);
    if (interpreter.AllocateTensors() != kTfLiteOk) {
        ESP_LOGE(TAG, "Failed to allocate tensors! Your model might need more RAM than specified.");
        heap_caps_free(tensor_arena);
        vTaskDelete(NULL);
        return;
    }

    TfLiteTensor* input_tensor = interpreter.input(0);
    TfLiteTensor* output_tensor = interpreter.output(0);
    ESP_LOGI(TAG, "TFLM Interpreter initialized.");

    // --- Sao chép dữ liệu test vào model ---
    size_t expected_input_size = sizeof(g_test_data);
    if (input_tensor->bytes != expected_input_size) {
        ESP_LOGE(TAG, "Input tensor size mismatch! Expected %d, but model requires %d", (int)expected_input_size, (int)input_tensor->bytes);
        heap_caps_free(tensor_arena);
        vTaskDelete(NULL);
        return;
    }
    memcpy(input_tensor->data.f, g_test_data, input_tensor->bytes);
    
    ESP_LOGI(TAG, "Running inference benchmark...");

    // === ĐÃ SỬA LỖI: Hoàn toàn vô hiệu hóa WDT trước khi chạy tác vụ dài ===
    // Điều này là cần thiết vì suy luận float32 rất chậm và sẽ chiếm dụng CPU,
    // ngăn không cho cả tác vụ IDLE reset watchdog của chính nó.
    ESP_LOGI(TAG, "De-initializing WDT before inference...");
    esp_task_wdt_deinit();

    // --- Chạy và đo thời gian ---
    int64_t start_time = esp_timer_get_time();
    
    if (interpreter.Invoke() != kTfLiteOk) {
        ESP_LOGE(TAG, "Invoke failed!");
    }

    int64_t end_time = esp_timer_get_time();
    int64_t duration_us = end_time - start_time;
    
    // === ĐÃ SỬA LỖI: Kích hoạt lại WDT với cấu hình mặc định sau khi hoàn thành ===
    ESP_LOGI(TAG, "Re-initializing WDT after inference...");
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = 5000,
        .idle_core_mask = (1 << 0) | (1 << 1), // Giám sát IDLE task trên cả hai core
        .trigger_panic = true,
    };
    ESP_ERROR_CHECK(esp_task_wdt_init(&wdt_config));

    // --- Giải phóng bộ nhớ Tensor Arena ---
    heap_caps_free(tensor_arena);
    ESP_LOGI(TAG, "Tensor Arena memory freed.");

    // --- Xử lý và in kết quả ---
    float* output_array = output_tensor->data.f;
    int predicted_class = -1;
    float max_prob = -1.0f;
    for (int i = 0; i < 5; i++) { // Giả sử model có 5 lớp đầu ra
        if (output_array[i] > max_prob) {
            max_prob = output_array[i];
            predicted_class = i;
        }
    }
    
    printf("\n\n--- OFFLINE BENCHMARK RESULT ---\n");
    printf("Platform: ESP32-S3 (Core %d)\n", xPortGetCoreID());
    printf("Predicted Action: %d (Confidence: %.3f)\n", predicted_class + 1, max_prob);
    printf("PURE INFERENCE time: %lld microseconds\n\n", duration_us);

    ESP_LOGI(TAG, "Benchmark finished. Deleting task...");
    vTaskDelete(NULL); // Tự xóa tác vụ sau khi hoàn thành
}

// ==============================================================================
// hàm app_main giờ đây chỉ có nhiệm vụ tạo tác vụ benchmark trên Core 1
// ==============================================================================
extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "app_main started on CPU %d.", xPortGetCoreID());

    // Tạo tác vụ benchmark và ghim nó vào Core 1
    // Core 0: Dành cho các tác vụ hệ thống
    // Core 1: Dành riêng cho tác vụ suy luận AI
    xTaskCreatePinnedToCore(
        benchmark_task,      // Con trỏ đến hàm của tác vụ
        "BenchmarkTask",     // Tên của tác vụ
        32768,               // Kích thước stack (128KB)
        NULL,                // Tham số truyền vào tác vụ
        5,                   // Độ ưu tiên
        NULL,                // Handle của tác vụ (không cần)
        1                    // ID của Core để ghim tác vụ vào (0 hoặc 1)
    );
    
    ESP_LOGI(TAG, "Benchmark task created on Core 1. app_main will now idle.");
}
