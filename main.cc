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

/* Linux includes */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <xkbcommon/xkbcommon-compose.h>

/* Genode includes */
#include <util/xml_generator.h>
#include <util/retry.h>
#include <util/reconstructible.h>

#include "xkb_mapping.h"

using Genode::Xml_generator;
using Genode::Constructible;


static void append_comment(Xml_generator &xml, char const *prefix,
                           char const *comment, char const *suffix)
{
	xml.append(prefix);
	xml.append("<!-- "); xml.append(comment); xml.append(" -->");
	xml.append(suffix);
}


/*
 * XML generator that expands to your needs
 */
class Expanding_xml_buffer
{
	private:

		char const *_name;

		enum { BUFFER_INCREMENT = 1024*1024 };

		size_t  _buffer_size { 0 };
		char   *_buffer      { nullptr };

		void _increase_buffer()
		{
			::free(_buffer);

			_buffer_size += BUFFER_INCREMENT;
			_buffer       = (char *)::calloc(1, _buffer_size);
		}

	public:

		Expanding_xml_buffer() { _increase_buffer(); }

		~Expanding_xml_buffer() { ::free(_buffer); }

		char const *buffer() const { return _buffer; }

		template <typename FUNC>
		void generate(char const *name, FUNC const &func)
		{
			Genode::retry<Xml_generator::Buffer_exceeded>(
				[&] () {
					Xml_generator xml(_buffer, _buffer_size,
					                  name, [&] () { func(xml); });
				},
				[&] () { _increase_buffer(); }
			);
		}
};


struct Utf8_for_key
{
	char _b[7] { 0, 0, 0, 0, 0, 0, 0 };

	Utf8_for_key(xkb_state *state, Input::Keycode code)
	{
		xkb_state_key_get_utf8(state, Xkb::keycode(code), _b, sizeof(_b));
	}

	bool valid() const { return _b[0] != 0; }
	char const *b() const { return _b; }

	void attributes(Xml_generator &xml)
	{
		for (unsigned i = 0; _b[i] && i < sizeof(_b); ++i) {
			char const bi[] = { 'b', char('0' + i), 0 };
			xml.attribute(bi, (unsigned char)_b[i]);
		}
	}
};


template <Input::Keycode code>
struct Locked
{
	xkb_state *state;

	Locked(xkb_state *state) : state(state)
	{
		xkb_state_update_key(state, Xkb::keycode(code), XKB_KEY_DOWN);
		xkb_state_update_key(state, Xkb::keycode(code), XKB_KEY_UP);
	}

	~Locked()
	{
		xkb_state_update_key(state, Xkb::keycode(code), XKB_KEY_DOWN);
		xkb_state_update_key(state, Xkb::keycode(code), XKB_KEY_UP);
	}
};


template <Input::Keycode code>
struct Pressed
{
	xkb_state *state;

	Pressed(xkb_state *state) : state(state)
	{
		xkb_state_update_key(state, Xkb::keycode(code), XKB_KEY_DOWN);
	}

	~Pressed()
	{
		xkb_state_update_key(state, Xkb::keycode(code), XKB_KEY_UP);
	}
};


struct Args
{
	struct Invalid_args { };

	enum class Command { GENERATE, DUMP, INFO };

	Command     command;
	char const *layout;
	char const *variant;
	char const *locale;

	char const *usage =
		"usage: xkb2ifcfg <command> <layout> <variant> <locale>\n"
		"\n"
		"  Commands\n"
		"\n"
		"    generate   generate input_filter config\n"
		"    dump       dump raw XKB keymap\n"
		"    info       simple per-key information\n"
		"\n"
		"  Example\n"
		"\n"
		"    xkb2ifcfg generate us ''         en_US.UTF-8\n"
		"    xkb2ifcfg info     de nodeadkeys de_DE.UTF-8\n";

	Args(int argc, char **argv)
	try {
		if (argc != 5) throw Invalid_args();

		if      (!::strcmp("generate", argv[1])) command = Command::GENERATE;
		else if (!::strcmp("dump",     argv[1])) command = Command::DUMP;
		else if (!::strcmp("info",     argv[1])) command = Command::INFO;
		else throw Invalid_args();

		layout  = argv[2];
		variant = argv[3];
		locale  = argv[4];
	} catch (...) { ::fputs(usage, stderr); throw; }
};


class Main
{
	private:

		struct Map;

		Args args;

		xkb_context       *_context;
		xkb_rule_names     _rmlvo;
		xkb_keymap        *_keymap;
		xkb_state         *_state;
		xkb_compose_table *_compose_table;
		xkb_compose_state *_compose_state;

		/*
		 * Numpad keys are remapped in input_filter if numlock=off, so we
		 * always assume numlock=on to handle KP1 etc. correctly.
		 */
		Constructible<Locked<Input::KEY_NUMLOCK>> _numlock;

		/* utilities */
		char const * _string(enum xkb_compose_status);
		char const * _string(enum xkb_compose_feed_result);

		bool _keysym_composing(xkb_keysym_t);

		void _keycode_info(xkb_keycode_t);
		void _keycode_xml_non_printable(Xml_generator &, xkb_keycode_t);
		void _keycode_xml_printable(Xml_generator &, xkb_keycode_t);
		void _keycode_xml_printable_shift(Xml_generator &, xkb_keycode_t);
		void _keycode_xml_printable_altgr(Xml_generator &, xkb_keycode_t);
		void _keycode_xml_printable_capslock(Xml_generator &, xkb_keycode_t);
		void _keycode_xml_printable_shift_altgr(Xml_generator &, xkb_keycode_t);
		void _keycode_xml_printable_shift_capslock(Xml_generator &, xkb_keycode_t);
		void _keycode_xml_printable_altgr_capslock(Xml_generator &, xkb_keycode_t);
		void _keycode_xml_printable_shift_altgr_capslock(Xml_generator &, xkb_keycode_t);

		int _generate();
		int _dump();
		int _info();

	public:

		Main(int argc, char **argv);
		~Main();

		xkb_keymap * keymap() { return _keymap; }

		int exec();
};


struct Main::Map
{
	Main          &main;
	Xml_generator &xml;

	enum class Mod : unsigned {
		NONE                 = 0,
		SHIFT                = 1, /* mod1 */
		                          /* mod2 is CTRL */
		ALTGR                = 4, /* mod3 */
		CAPSLOCK             = 8, /* mod4 */
		SHIFT_ALTGR          = SHIFT | ALTGR,
		SHIFT_CAPSLOCK       = SHIFT | CAPSLOCK,
		ALTGR_CAPSLOCK       = ALTGR | CAPSLOCK,
		SHIFT_ALTGR_CAPSLOCK = SHIFT | ALTGR | CAPSLOCK,
	} mod;

	static char const * _string(Mod mod)
	{
		switch (mod) {
		case Map::Mod::NONE:                 return "no modifier";
		case Map::Mod::SHIFT:                return "SHIFT";
		case Map::Mod::ALTGR:                return "ALTGR";
		case Map::Mod::CAPSLOCK:             return "CAPSLOCK";
		case Map::Mod::SHIFT_ALTGR:          return "SHIFT-ALTGR";
		case Map::Mod::SHIFT_CAPSLOCK:       return "SHIFT-CAPSLOCK";
		case Map::Mod::ALTGR_CAPSLOCK:       return "ALTGR-CAPSLOCK";
		case Map::Mod::SHIFT_ALTGR_CAPSLOCK: return "SHIFT-ALTGR-CAPSLOCK";
		}
		return "invalid";
	}

	static void _non_printable(xkb_keymap *, xkb_keycode_t keycode, void *data)
	{
		Map &m = *reinterpret_cast<Map *>(data);

		if (m.mod != Map::Mod::NONE) {
			::printf("%s: mod=%u not supported\n",
			       __PRETTY_FUNCTION__, unsigned(m.mod));
			return;
		}

		m.main._keycode_xml_non_printable(m.xml, keycode);
	};

	static void _printable(xkb_keymap *, xkb_keycode_t keycode, void *data)
	{
		Map &m = *reinterpret_cast<Map *>(data);

		switch (m.mod) {
		case Map::Mod::NONE:
			m.main._keycode_xml_printable(m.xml, keycode); break;
		case Map::Mod::SHIFT:
			m.main._keycode_xml_printable_shift(m.xml, keycode); break;
		case Map::Mod::ALTGR:
			m.main._keycode_xml_printable_altgr(m.xml, keycode); break;
		case Map::Mod::CAPSLOCK:
			m.main._keycode_xml_printable_capslock(m.xml, keycode); break;
		case Map::Mod::SHIFT_ALTGR:
			m.main._keycode_xml_printable_shift_altgr(m.xml, keycode); break;
		case Map::Mod::SHIFT_CAPSLOCK:
			m.main._keycode_xml_printable_shift_capslock(m.xml, keycode); break;
		case Map::Mod::ALTGR_CAPSLOCK:
			m.main._keycode_xml_printable_altgr_capslock(m.xml, keycode); break;
		case Map::Mod::SHIFT_ALTGR_CAPSLOCK:
			m.main._keycode_xml_printable_shift_altgr_capslock(m.xml, keycode); break;
		}
	};

	Map(Main &main, Xml_generator &xml, Mod mod)
	:
		main(main), xml(xml), mod(mod)
	{
		if (mod == Mod::NONE) {
			/* generate basic character map */
			xml.node("map", [&] ()
			{
				append_comment(xml, "\n\t\t", "printable", "");
				xkb_keymap_key_for_each(main.keymap(), _printable, this);

				append_comment(xml, "\n\n\t\t", "non-printable", "");
				xkb_keymap_key_for_each(main.keymap(), _non_printable, this);
			});

		} else {

			/* generate characters depending on modifier state */
			append_comment(xml, "\n\n\t", _string(mod), "");
			xml.node("map", [&] ()
			{
				xml.attribute("mod1", (bool)(unsigned(mod) & unsigned(Mod::SHIFT)));
				xml.attribute("mod3", (bool)(unsigned(mod) & unsigned(Mod::ALTGR)));
				xml.attribute("mod4", (bool)(unsigned(mod) & unsigned(Mod::CAPSLOCK)));

				xkb_keymap_key_for_each(main.keymap(), _printable, this);
				/* FIXME xml.append() as last operation breaks indentation */
				xml.node("end", [] () {});
			});
		}
	}
};


char const * Main::_string(enum xkb_compose_status status)
{
    switch (status) {
    case XKB_COMPOSE_NOTHING:   return "XKB_COMPOSE_NOTHING";
    case XKB_COMPOSE_COMPOSING: return "XKB_COMPOSE_COMPOSING";
    case XKB_COMPOSE_COMPOSED:  return "XKB_COMPOSE_COMPOSED";
    case XKB_COMPOSE_CANCELLED: return "XKB_COMPOSE_CANCELLED";
    }
    return "invalid";
}


char const * Main::_string(enum xkb_compose_feed_result result)
{
    switch (result) {
    case XKB_COMPOSE_FEED_IGNORED:  return "XKB_COMPOSE_FEED_IGNORED";
    case XKB_COMPOSE_FEED_ACCEPTED: return "XKB_COMPOSE_FEED_ACCEPTED";
    }
    return "invalid";
}


bool Main::_keysym_composing(xkb_keysym_t sym)
{
	xkb_compose_state_reset(_compose_state);
	xkb_compose_state_feed(_compose_state, sym);

	return (XKB_COMPOSE_COMPOSING == xkb_compose_state_get_status(_compose_state));
}


void Main::_keycode_info(xkb_keycode_t keycode)
{
	for (Xkb::Mapping &m : Xkb::printable) {
		if (m.xkb != keycode) continue;

		::printf("keycode %3u:", m.xkb);
		::printf(" %-8s", m.xkb_name);
		::printf(" %-16s", Input::key_name(m.code));

		unsigned const num_levels = xkb_keymap_num_levels_for_key(_keymap, m.xkb, 0);
		::printf("\t%u levels { ", num_levels);

		for (unsigned l = 0; l < num_levels; ++l) {
			::printf(" %u:", l);

			xkb_keysym_t const *syms = nullptr;
			unsigned const num_syms = xkb_keymap_key_get_syms_by_level(_keymap, m.xkb, 0, l, &syms);

			for (unsigned s = 0; s < num_syms; ++s) {
				char buffer[7] = { 0, };
				xkb_keysym_to_utf8(syms[s], buffer, sizeof(buffer));
				::printf(" %x %s", syms[s], _keysym_composing(syms[s]) ? "COMPOSING!" : buffer);
			}
		}

		::printf(" }");
		::printf("\n");
		return;
	}
}


void Main::_keycode_xml_non_printable(Xml_generator &xml, xkb_keycode_t keycode)
{
	/* non-printable symbols with chargen entry (e.g., ENTER) */
	for (Xkb::Mapping &m : Xkb::non_printable) {
		if (m.xkb != keycode) continue;

		Utf8_for_key utf8(_state, m.code);

		xml.node("key", [&] ()
		{
			xml.attribute("name", Input::key_name(m.code));
			/* FIXME produces ascii 13 for ENTER not 10 */
			xml.attribute("ascii", (unsigned char)utf8.b()[0]);
		});

		return;
	}
}


void Main::_keycode_xml_printable(Xml_generator &xml, xkb_keycode_t keycode)
{
	for (Xkb::Mapping &m : Xkb::printable) {
		if (m.xkb != keycode) continue;

		xkb_keysym_t keysym = xkb_state_key_get_one_sym(_state, m.xkb);
		if (keysym != XKB_KEY_NoSymbol && _keysym_composing(keysym)) {
			char buffer[256];
			xkb_keysym_get_name(keysym, buffer, sizeof(buffer));
			::fprintf(stderr, "unsupported composing keysym <%s> on %s\n",
			          buffer, Input::key_name(m.code));
		}

		Utf8_for_key utf8(_state, m.code);

		if (utf8.valid()) {
			xml.node("key", [&] ()
			{
				xml.attribute("name", Input::key_name(m.code));
				utf8.attributes(xml);
			});
			/* FIXME make the comment optional */
			append_comment(xml, "\t", utf8.b(), "");
		}

		return;
	}
}

void Main::_keycode_xml_printable_shift(Xml_generator &xml, xkb_keycode_t keycode)
{
	Pressed<Input::KEY_LEFTSHIFT> shift(_state);
	_keycode_xml_printable(xml, keycode);
}


void Main::_keycode_xml_printable_altgr(Xml_generator &xml, xkb_keycode_t keycode)
{
	Pressed<Input::KEY_RIGHTALT> altgr(_state);
	_keycode_xml_printable(xml, keycode);
}


void Main::_keycode_xml_printable_capslock(Xml_generator &xml, xkb_keycode_t keycode)
{
	Locked<Input::KEY_CAPSLOCK> capslock(_state);
	_keycode_xml_printable(xml, keycode);
}


void Main::_keycode_xml_printable_shift_altgr(Xml_generator &xml, xkb_keycode_t keycode)
{
	Pressed<Input::KEY_LEFTSHIFT> shift(_state);
	Pressed<Input::KEY_RIGHTALT>  altgr(_state);
	_keycode_xml_printable(xml, keycode);
}


void Main::_keycode_xml_printable_shift_capslock(Xml_generator &xml, xkb_keycode_t keycode)
{
	Locked<Input::KEY_CAPSLOCK>   capslock(_state);
	Pressed<Input::KEY_LEFTSHIFT> shift(_state);
	_keycode_xml_printable(xml, keycode);
}


void Main::_keycode_xml_printable_altgr_capslock(Xml_generator &xml, xkb_keycode_t keycode)
{
	Locked<Input::KEY_CAPSLOCK>  capslock(_state);
	Pressed<Input::KEY_RIGHTALT> altgr(_state);
	_keycode_xml_printable(xml, keycode);
}


void Main::_keycode_xml_printable_shift_altgr_capslock(Xml_generator &xml, xkb_keycode_t keycode)
{
	Locked<Input::KEY_CAPSLOCK>   capslock(_state);
	Pressed<Input::KEY_LEFTSHIFT> shift(_state);
	Pressed<Input::KEY_RIGHTALT>  altgr(_state);
	_keycode_xml_printable(xml, keycode);
}


int Main::_generate()
{
	::printf("<!-- %s-%s-%s chargen configuration generated by xkb2ifcfg -->\n",
	         args.layout, args.variant, args.locale);

	Expanding_xml_buffer xml_buffer;

	auto generate_xml = [&] (Xml_generator &xml)
	{
		{ Map map { *this, xml, Map::Mod::NONE }; }
		{ Map map { *this, xml, Map::Mod::SHIFT }; }
		{ Map map { *this, xml, Map::Mod::ALTGR }; }
		{ Map map { *this, xml, Map::Mod::CAPSLOCK }; }
		{ Map map { *this, xml, Map::Mod::SHIFT_ALTGR }; }
		{ Map map { *this, xml, Map::Mod::SHIFT_CAPSLOCK }; }
		{ Map map { *this, xml, Map::Mod::ALTGR_CAPSLOCK }; }
		{ Map map { *this, xml, Map::Mod::SHIFT_ALTGR_CAPSLOCK }; }
	};

	xml_buffer.generate("chargen", generate_xml);

	::puts(xml_buffer.buffer());

	return 0;
}


int Main::_dump()
{
	::printf("Dump of XKB keymap for %s-%s-%s by xkb2ifcfg\n",
	         args.layout, args.variant, args.locale);
	::puts(xkb_keymap_get_as_string(_keymap, XKB_KEYMAP_FORMAT_TEXT_V1));

	return 0;
}


int Main::_info()
{
	::printf("Simple per-key info for %s-%s-%s by xkb2ifcfg\n",
	         args.layout, args.variant, args.locale);

	auto lambda = [] (xkb_keymap *, xkb_keycode_t keycode, void *data)
	{
		reinterpret_cast<Main *>(data)->_keycode_info(keycode);
	};

	xkb_keymap_key_for_each(_keymap, lambda, this);

	return 0;
}


int Main::exec()
{
	switch (args.command) {
	case Args::Command::GENERATE: return _generate();
	case Args::Command::DUMP:     return _dump();
	case Args::Command::INFO:     return _info();
	}

	return -1;
}


Main::Main(int argc, char **argv) : args(argc, argv)
{
	/* TODO error handling */
	_context       = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	_rmlvo         = { "evdev", "pc105", args.layout, args.variant, "" };
	_keymap        = xkb_keymap_new_from_names(_context, &_rmlvo, XKB_KEYMAP_COMPILE_NO_FLAGS);
	_state         = xkb_state_new(_keymap);
	_compose_table = xkb_compose_table_new_from_locale(_context, args.locale, XKB_COMPOSE_COMPILE_NO_FLAGS);
	_compose_state = xkb_compose_state_new(_compose_table, XKB_COMPOSE_STATE_NO_FLAGS);

	_numlock.construct(_state);
}


Main::~Main()
{
	_numlock.destruct();

	xkb_compose_state_unref(_compose_state);
	xkb_compose_table_unref(_compose_table);
	xkb_state_unref(_state);
	xkb_keymap_unref(_keymap);
	xkb_context_unref(_context);
}


int main(int argc, char **argv)
{
	try {
		static Main m(argc, argv);

		return m.exec();
	} catch (...) { return -1; }
}
