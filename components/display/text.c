/* 
 *  (c) 2004,2006 Richard Titmuss for SlimProtoLib 
 *  (c) Philippe G. 2019, philippe_44@outlook.com
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <string.h>
#include <ctype.h>
#include <arpa/inet.h>

#include "esp_log.h"

#define LINELEN 60
#define TAG "display"

//Change special LCD chars to something more printable on screen
unsigned char printable(unsigned char c) {
   switch (c) {
	  case 11:		/* block */
		 return '#';
	 break;;
	  case 16:		/* rightarrow */
		 return '>';
	 break;;
	  case 22:		/* circle */
		 return '@';
	 break;;
	  case 145:		/* note */
	 return ' ';
	 break;;
	  case 152:		/* bell */
		 return 'o';
	 break;;
	  default:
		 return c;
	  }
}

// Replace unprintable symbols in line
void makeprintable(unsigned char * line) {
	for (int n=0; n < LINELEN; n++) line[n] = printable(line[n]);
}

// Show the display
void show_display_buffer(char *ddram) {
	char line1[LINELEN+1];
	char *line2;

	memset(line1, 0, LINELEN+1);
	strncpy(line1, ddram, LINELEN);
	line2 = &(ddram[LINELEN]);
	line2[LINELEN] = '\0';

	/* Convert special LCD chars */
	makeprintable((unsigned char *)line1);
	makeprintable((unsigned char *)line2);
	ESP_LOGI(TAG, "\n\t%.40s\n\t%.40s", line1, line2);
}

// Check if char is printable, or a valid symbol
bool charisok(unsigned char c) {
   switch (c) {
	  case 11:		/* block */
	  case 16:		/* rightarrow */
	  case 22:		/* circle */
	  case 145:		/* note */
	  case 152:		/* bell */
		 return true;
	 break;;
	  default:
		 return isprint(c);
   }
}

// Process display data
void vfd_data( unsigned short *data, int bytes_read) {
	unsigned short *display_data;
	char ddram[LINELEN * 2];
	int n;
	int addr = 0; /* counter */

	if (bytes_read % 2) bytes_read--; /* even number of bytes */
	display_data = &(data[6]); /* display data starts at byte 12 */

	memset(ddram, ' ', LINELEN * 2);
	for (n=0; n<(bytes_read/2); n++) {
		unsigned short d; /* data element */
		unsigned char t, c;

		d = ntohs(display_data[n]);
		t = (d & 0x00ff00) >> 8; /* type of display data */
		c = (d & 0x0000ff); /* character/command */
		switch (t) {
			case 0x03: /* character */
				if (!charisok(c)) c = ' ';
				if (addr <= LINELEN * 2) {
					ddram[addr++] = c;
		}
				break;
			case 0x02: /* command */
				switch (c) {
					case 0x06: /* display clear */
						memset(ddram, ' ', LINELEN * 2);
			break;
					case 0x02: /* cursor home */
						addr = 0;
						break;
					case 0xc0: /* cursor home2 */
						addr = LINELEN;
						break;
				}
		}
	}
	show_display_buffer(ddram);
}
