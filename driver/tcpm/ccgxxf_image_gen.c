#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define CCGXXFCYACD_FILE	"./TCPC_CCG6DF_B147_0_1_1.cyacd"
#define CCGXXFCYACD_READ_LEN	300
#define CCGXXFCYACD_ROW_SIZE	128
#define CCGXXFCYACD_ROW_INDEX	11

static uint8_t str_to_uchar(char ch)
{
	uint8_t val;

	if (ch >= '0' && ch <= '9')
		val = ch - 48;
	else
		val = 0xA + ch - 65;

	return val;
}

static void ccgxxf_fill_image(uint8_t *data, char *line)
{
	int i, j = CCGXXFCYACD_ROW_INDEX;

	for (i = 0; i < CCGXXFCYACD_ROW_SIZE; i++, j += 2)
		data[i] = (str_to_uchar(line[j]) << 4) |
				str_to_uchar(line[j + 1]);
}

static void ccgxxf_print_image(uint8_t *data, int rows)
{
	int i, j, k;
	uint8_t *ptr = data;

	for (i = 0; i < rows; i++) {
		printf("Row %d\n", i);
		for (j = 0; j < 8; j++) {
			for (k = 0; k < 16; k++)
				printf("0x%02x,", *ptr++);
			printf("\n");
		}
	}
}

int main()
{
	FILE *f;
	char line[CCGXXFCYACD_READ_LEN], ch;
	uint8_t *meta_data;
	uint8_t *image_data;
	int rows = 0, fline = 0;

	/* Open the file */
	f = fopen(CCGXXFCYACD_FILE, "r");
	if (!f) {
		printf("Can't open CYACD file %s\n", CCGXXFCYACD_FILE);
		return -1;
	}

	/* Get the number of lines in the file */
	for (ch = getc(f); ch != EOF; ch = getc(f)) {
		if (ch == '\n')
			fline++;
	}

	/* First line is attributes & one line is metadata */
	image_data = (uint8_t *) malloc((fline - 2) * CCGXXFCYACD_ROW_SIZE);
	meta_data = (uint8_t *) malloc(CCGXXFCYACD_ROW_SIZE);

	/* Skip first line with attributes */
	fseek(f, 0, SEEK_SET);
	fgets(line, CCGXXFCYACD_READ_LEN, f);

	while(!feof(f)) {
		fgets(line, CCGXXFCYACD_READ_LEN, f);

		/* Fill the metadata */
		if (line[4] == '1' && line[5] == 'F' &&
			(line[6] == 'E' || line[6] == 'F')) {
			ccgxxf_fill_image(meta_data, line);
			break;
		}

		/* Fill the image data */ 
		ccgxxf_fill_image(
			&image_data[rows * CCGXXFCYACD_ROW_SIZE], line);
		rows++;
	}

	ccgxxf_print_image(image_data, rows); 
	ccgxxf_print_image(meta_data, 1); 

	free(meta_data);
	free(image_data);
	fclose(f);

	return 0;
}
