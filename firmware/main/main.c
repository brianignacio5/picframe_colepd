/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain 
 * this notice you can do whatever you want with this stuff. If we meet some day, 
 * and you think this stuff is worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
*/

/*
 * Hello World Example - Minimal ESP32C3 Test
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "spi_flash_mmap.h"
#include "epd.h"
#include "epd_flash_image.h"

static const char *TAG = "epd_test";

void app_main(void)
{
	ESP_LOGI(TAG, "=== EPD Image Loading Test ===");
	ESP_LOGI(TAG, "ESP32C3 with partition image loading...");
	
	// Access embedded icon data
	extern const uint8_t icons_bmp_start[] asm("_binary_icons_bmp_start");
	extern const uint8_t icons_bmp_end[] asm("_binary_icons_bmp_end");
	const uint8_t *icon = icons_bmp_start;
	size_t icon_size = icons_bmp_end - icons_bmp_start;
	
	ESP_LOGI(TAG, "Icon embedded successfully, size: %d bytes", icon_size);
	ESP_LOGI(TAG, "Icon will be used for EPD status display");
	
	// Test image partition loading
	ESP_LOGI(TAG, "Testing image partition access...");
	
	// Find the images partition (type 123, subtype 0)
	const esp_partition_t *part = esp_partition_find_first(123, 0, NULL);
	if (part != NULL) {
		ESP_LOGI(TAG, "Found images partition: size=%lu bytes, offset=0x%lx", (unsigned long)part->size, (unsigned long)part->address);
		
		// Map the partition to access image data
		const flash_image_t *images = NULL;
		spi_flash_mmap_handle_t mmap_handle;
		esp_err_t err = esp_partition_mmap(part, 0, IMG_SIZE_BYTES*IMG_SLOT_COUNT, SPI_FLASH_MMAP_DATA, (const void**)&images, &mmap_handle);
		
		if (err == ESP_OK) {
			ESP_LOGI(TAG, "Partition mapped successfully, checking for images...");
			ESP_LOGI(TAG, "Looking for %d image slots of %d bytes each", IMG_SLOT_COUNT, IMG_SIZE_BYTES);
			
			// Check each image slot
			bool found_image = false;
			for (int i = 0; i < IMG_SLOT_COUNT; i++) {
				ESP_LOGI(TAG, "Slot %d: header magic = 0x%08lx", i, (unsigned long)images[i].hdr.id);
				
				if (img_valid(&images[i].hdr)) {
					ESP_LOGI(TAG, "✅ Found valid image in slot %d!", i);
					ESP_LOGI(TAG, "   Magic: 0x%08lx", (unsigned long)images[i].hdr.id);
					ESP_LOGI(TAG, "   Timestamp: %llu", (unsigned long long)images[i].hdr.timestamp);
					ESP_LOGI(TAG, "   Data size: %d bytes", 600*448/2);
					found_image = true;
					
					// Step 4: Send to EPD!
					ESP_LOGI(TAG, "🖼️  Sending image to EPD...");
					epd_send(images[i].data, ICON_NONE);
					ESP_LOGI(TAG, "📺 EPD display complete!");
					
					ESP_LOGI(TAG, "💤 Shutting down EPD...");
					epd_shutdown();
					ESP_LOGI(TAG, "✅ EPD test successful!");
					break;
				} else {
					ESP_LOGI(TAG, "   Slot %d: Invalid or empty", i);
				}
			}
			
			if (!found_image) {
				ESP_LOGW(TAG, "No valid images found in partition");
				ESP_LOGI(TAG, "Expected magic: 0x%08lx", 0xfafa1a1aUL);
			}
			
			// Unmap the partition
			spi_flash_munmap(mmap_handle);
		} else {
			ESP_LOGE(TAG, "Failed to map partition: %s", esp_err_to_name(err));
		}
	} else {
		ESP_LOGE(TAG, "Images partition not found!");
	}
	
	ESP_LOGI(TAG, "Image loading test complete");
	
	while (1) {
		ESP_LOGI(TAG, "Image loading test running...");
		vTaskDelay(10000 / portTICK_PERIOD_MS);
	}
}
