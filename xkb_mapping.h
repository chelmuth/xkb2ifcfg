/*
 * \brief  Libxkbcommon-based keyboard-layout generator
 * \author Christian Helmuth <christian.helmuth@genode-labs.com>
 * \date   2019-07-16
 *
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef _XKB_MAPPING_H_
#define _XKB_MAPPING_H_

/* Linux includes */
#include <xkbcommon/xkbcommon.h>

/* Genode includes */
#include <input/keycodes.h>


namespace Xkb {

	/*
	 * It's a documented fact that 'xkb keycode == evdev keycode + 8'
	 */
	inline xkb_keycode_t keycode(Input::Keycode code)
	{
		return xkb_keycode_t(unsigned(code) + 8);
	}

	/*
	 * Lookup table for keys eventually generating characters
	 */
	struct Mapping
	{
		xkb_keycode_t  xkb;
		char const     xkb_name[7];
		Input::Keycode code;
//		char const     ascii; /* non-printable */
	};

	Mapping printable[] = {
		{ 10,  "<AE01>", Input::KEY_1 },
		{ 11,  "<AE02>", Input::KEY_2 },
		{ 12,  "<AE03>", Input::KEY_3 },
		{ 13,  "<AE04>", Input::KEY_4 },
		{ 14,  "<AE05>", Input::KEY_5 },
		{ 15,  "<AE06>", Input::KEY_6 },
		{ 16,  "<AE07>", Input::KEY_7 },
		{ 17,  "<AE08>", Input::KEY_8 },
		{ 18,  "<AE09>", Input::KEY_9 },
		{ 19,  "<AE10>", Input::KEY_0 },
		{ 20,  "<AE11>", Input::KEY_MINUS },
		{ 21,  "<AE12>", Input::KEY_EQUAL },

		{ 24,  "<AD01>", Input::KEY_Q },
		{ 25,  "<AD02>", Input::KEY_W },
		{ 26,  "<AD03>", Input::KEY_E },
		{ 27,  "<AD04>", Input::KEY_R },
		{ 28,  "<AD05>", Input::KEY_T },
		{ 29,  "<AD06>", Input::KEY_Y },
		{ 30,  "<AD07>", Input::KEY_U },
		{ 31,  "<AD08>", Input::KEY_I },
		{ 32,  "<AD09>", Input::KEY_O },
		{ 33,  "<AD10>", Input::KEY_P },
		{ 34,  "<AD11>", Input::KEY_LEFTBRACE  },
		{ 35,  "<AD12>", Input::KEY_RIGHTBRACE },

		{ 38,  "<AC01>", Input::KEY_A },
		{ 39,  "<AC02>", Input::KEY_S },
		{ 40,  "<AC03>", Input::KEY_D },
		{ 41,  "<AC04>", Input::KEY_F },
		{ 42,  "<AC05>", Input::KEY_G },
		{ 43,  "<AC06>", Input::KEY_H },
		{ 44,  "<AC07>", Input::KEY_J },
		{ 45,  "<AC08>", Input::KEY_K },
		{ 46,  "<AC09>", Input::KEY_L },
		{ 47,  "<AC11>", Input::KEY_SEMICOLON  },
		{ 48,  "<AC12>", Input::KEY_APOSTROPHE },

		{ 49,  "<TLDE>", Input::KEY_GRAVE },      /* left of "1" <AE01> */
		{ 51,  "<BKSL>", Input::KEY_BACKSLASH  }, /* left of <RTRN> (pc105) / above <RTRN> (pc104) */

		{ 52,  "<AB01>", Input::KEY_Z },
		{ 53,  "<AB02>", Input::KEY_X },
		{ 54,  "<AB03>", Input::KEY_C },
		{ 55,  "<AB04>", Input::KEY_V },
		{ 56,  "<AB05>", Input::KEY_B },
		{ 57,  "<AB06>", Input::KEY_N },
		{ 58,  "<AB07>", Input::KEY_M },
		{ 59,  "<AB08>", Input::KEY_COMMA },
		{ 60,  "<AB09>", Input::KEY_DOT },
		{ 61,  "<AB10>", Input::KEY_SLASH },

		{ 65,  "<SPCE>", Input::KEY_SPACE },
		{ 94,  "<LSGT>", Input::KEY_102ND }, /* right of <LFSH> (pc105) */

		{ 63,  "<KPMU>", Input::KEY_KPASTERISK },
		{ 79,  "<KP7>",  Input::KEY_KP7 },
		{ 80,  "<KP8>",  Input::KEY_KP8 },
		{ 81,  "<KP9>",  Input::KEY_KP9 },
		{ 82,  "<KPSU>", Input::KEY_KPMINUS },
		{ 83,  "<KP4>",  Input::KEY_KP4 },
		{ 84,  "<KP5>",  Input::KEY_KP5 },
		{ 85,  "<KP6>",  Input::KEY_KP6 },
		{ 86,  "<KPAD>", Input::KEY_KPPLUS },
		{ 87,  "<KP1>",  Input::KEY_KP1 },
		{ 88,  "<KP2>",  Input::KEY_KP2 },
		{ 89,  "<KP3>",  Input::KEY_KP3 },
		{ 90,  "<KP0>",  Input::KEY_KP0 },
		{ 91,  "<KPDL>", Input::KEY_KPDOT },
		{ 106, "<KPDV>", Input::KEY_KPSLASH },
	};

	Mapping non_printable[] = {
		{ 9,   "<ESC>",  Input::KEY_ESC },
		{ 22,  "<BKSP>", Input::KEY_BACKSPACE },
		{ 23,  "<TAB>",  Input::KEY_TAB },
		{ 36,  "<RTRN>", Input::KEY_ENTER },
		{ 104, "<KPEN>", Input::KEY_KPENTER },
		{ 119, "<DELE>", Input::KEY_DELETE },
	};

}

#endif /* _XKB_MAPPING_H_ */
