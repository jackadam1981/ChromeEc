#include "task.h"
#include "util.h"
#include "system.h"

#include "hotword_dsp_api.h"
#include "okgoogle-16k.wav_hex.h"
#include "model.h"

static const int kFrameSize = 160;

static int test_hotword_detection_task(int argc, char **argv)
{
	int16_t *wave_buffer = (int16_t *)_hex_data;
	size_t wave_buffer_length = _hex_data_len / sizeof(int16_t);
	size_t total_read = 0;
	int frame_number = 0;
	int preamble_length_ms;

	if (!GoogleHotwordDspInit(*memmap_memory_banks)) {
		ccprintf("Unable to initialize Hotword!\n");
		return 1;
	}

	ccprintf("Hotword detection using library generated at CL# %d\n",
		 GoogleHotwordVersion());

	while (total_read < wave_buffer_length) {
		int hotword_detected = GoogleHotwordDspProcess(
				wave_buffer + total_read, kFrameSize, &preamble_length_ms);
		total_read += kFrameSize;
		if (hotword_detected) {
			ccprintf("Hotword detected with pre-trigger preamble length of %d ms @frame "
				 "%d at %d ms.\n",
				 preamble_length_ms, frame_number, frame_number / 10);
			GoogleHotwordDspReset();
		}
		frame_number++;
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(t, test_hotword_detection_task, "None", "Test");
