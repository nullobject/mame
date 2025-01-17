// license:BSD-3-Clause
// copyright-holders:Sandro Ronco
/***************************************************************************

        Casio PB-1000 / PB-2000c driver

        Driver by Sandro Ronco

        TODO:
        - improve the pb2000c gate array emulation
        - i/o port

        Known issues:
        - the second memory card is not recognized by the HW
        - pb2000c, ai1000: the shift key has to be released before hitting
          the key that is to be shifted. Therefore natural keyboard/paste
          will misbehave for shifted characters.

        More info:
            http://www.itkp.uni-bonn.de/~wichmann/pb1000-wrobel.html

****************************************************************************/


#include "emu.h"
#include "cpu/hd61700/hd61700.h"
#include "machine/nvram.h"
#include "sound/beep.h"
#include "video/hd44352.h"

#include "emupal.h"
#include "screen.h"
#include "softlist_dev.h"
#include "speaker.h"

#include "bus/generic/carts.h"
#include "bus/generic/slot.h"


class pb1000_state : public driver_device
{
public:
	pb1000_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag)
		, m_maincpu(*this, "maincpu")
		, m_beeper(*this, "beeper")
		, m_hd44352(*this, "hd44352")
		, m_card1(*this, "cardslot1")
		, m_card2(*this, "cardslot2")
		, m_io_keyboard(*this, "KO%u", 0U)
	{ }

	void pb2000c(machine_config &config);
	void pb1000(machine_config &config);

private:
	required_device<hd61700_cpu_device> m_maincpu;
	required_device<beep_device> m_beeper;
	required_device<hd44352_device> m_hd44352;
	optional_device<generic_slot_device> m_card1;
	optional_device<generic_slot_device> m_card2;
	required_ioport_array<13> m_io_keyboard;

	emu_timer *m_kb_timer = nullptr;
	uint8_t m_kb_matrix = 0;
	uint8_t m_gatearray[2]{};

	memory_region *m_rom_reg = nullptr;
	memory_region *m_card1_reg = nullptr;
	memory_region *m_card2_reg = nullptr;

	virtual void machine_start() override;
	void gatearray_w(offs_t offset, uint16_t data);
	uint16_t pb1000_kb_r();
	uint16_t pb2000c_kb_r();
	void kb_matrix_w(uint8_t data);
	uint8_t pb1000_port_r();
	uint8_t pb2000c_port_r();
	void port_w(uint8_t data);
	uint16_t read_touchscreen(uint8_t line);
	void pb1000_palette(palette_device &palette) const;
	TIMER_CALLBACK_MEMBER(keyboard_timer);
	void pb1000_mem(address_map &map);
	void pb2000c_mem(address_map &map);
};

void pb1000_state::pb1000_mem(address_map &map)
{
	map.unmap_value_low();
	map(0x00000, 0x00bff).rom();
	//map(0x00c00, 0x00c0f).noprw();   //I/O
	map(0x06000, 0x07fff).ram().share("nvram1");
	map(0x08000, 0x0ffff).bankr("bank1");
	map(0x18000, 0x1ffff).ram().share("nvram2");
}

void pb1000_state::pb2000c_mem(address_map &map)
{
	map.unmap_value_low();
	map(0x00000, 0x0ffff).bankr("bank1");
	map(0x00000, 0x00bff).rom();
	//map(0x00c00, 0x00c0f).noprw();   //I/O
	map(0x00c10, 0x00c11).w(FUNC(pb1000_state::gatearray_w));
	map(0x10000, 0x1ffff).ram().share("nvram1");
	map(0x20000, 0x27fff).r(m_card1, FUNC(generic_slot_device::read16_rom));
	map(0x28000, 0x2ffff).ram().share("nvram2");
}


/* Input ports */
static INPUT_PORTS_START( pb1000 )
	PORT_START("KO0")
	PORT_START("KO1")
		PORT_BIT(0x0080, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("BRK")     PORT_CODE(KEYCODE_F10)          PORT_CHAR(UCHAR_MAMEKEY(F10))
		PORT_BIT(0x0020, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("OFF")     PORT_CODE(KEYCODE_7_PAD)
	PORT_START("KO2")
		PORT_BIT(0x8000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("EXE")     PORT_CODE(KEYCODE_ENTER)        PORT_CHAR(13)
		PORT_BIT(0x4000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("IN")      PORT_CODE(KEYCODE_F4)           PORT_CHAR(UCHAR_MAMEKEY(F4))
		PORT_BIT(0x2000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("MEMO IN") PORT_CODE(KEYCODE_0_PAD)
		PORT_BIT(0x0080, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME(",  ?")    PORT_CODE(KEYCODE_COMMA)        PORT_CHAR(',')  PORT_CHAR('?')
		PORT_BIT(0x0020, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("\"  !")   PORT_CODE(KEYCODE_QUOTE)        PORT_CHAR('"')  PORT_CHAR('!')
		PORT_BIT(0x0010, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("$  #")    PORT_CODE(KEYCODE_1_PAD)        PORT_CHAR('$')  PORT_CHAR('#')
		PORT_BIT(0x0008, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("&  %")    PORT_CODE(KEYCODE_2_PAD)        PORT_CHAR('&')  PORT_CHAR('%')
		PORT_BIT(0x0004, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("=  '")    PORT_CODE(KEYCODE_EQUALS)       PORT_CHAR('=')  PORT_CHAR('\'')
		PORT_BIT(0x0002, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME(";  <")    PORT_CODE(KEYCODE_COLON)        PORT_CHAR(';')  PORT_CHAR('<')
		PORT_BIT(0x0001, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME(":  >")    PORT_CODE(KEYCODE_BACKSLASH2)   PORT_CHAR(':')  PORT_CHAR('>')
	PORT_START("KO3")
		PORT_BIT(0x8000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME(UTF8_RIGHT) PORT_CODE(KEYCODE_RIGHT)       PORT_CHAR(UCHAR_MAMEKEY(RIGHT))
		PORT_BIT(0x4000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("OUT")     PORT_CODE(KEYCODE_F3)           PORT_CHAR(UCHAR_MAMEKEY(F3))
		PORT_BIT(0x2000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("MEMO")    PORT_CODE(KEYCODE_F11)          PORT_CHAR(UCHAR_MAMEKEY(F11))
		PORT_BIT(0x0080, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("U")       PORT_CODE(KEYCODE_U)            PORT_CHAR('U')
		PORT_BIT(0x0020, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Q")       PORT_CODE(KEYCODE_Q)            PORT_CHAR('Q')
		PORT_BIT(0x0010, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("W")       PORT_CODE(KEYCODE_W)            PORT_CHAR('W')
		PORT_BIT(0x0008, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("E")       PORT_CODE(KEYCODE_E)            PORT_CHAR('E')
		PORT_BIT(0x0004, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("R")       PORT_CODE(KEYCODE_R)            PORT_CHAR('R')
		PORT_BIT(0x0002, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("T")       PORT_CODE(KEYCODE_T)            PORT_CHAR('T')
		PORT_BIT(0x0001, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Y")       PORT_CODE(KEYCODE_Y)            PORT_CHAR('Y')
	PORT_START("KO4")
		PORT_BIT(0x8000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME(UTF8_DOWN) PORT_CODE(KEYCODE_DOWN)         PORT_CHAR(UCHAR_MAMEKEY(DOWN))
		PORT_BIT(0x4000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("CALC")    PORT_CODE(KEYCODE_HOME)
		PORT_BIT(0x2000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("CAL")     PORT_CODE(KEYCODE_F9)           PORT_CHAR(UCHAR_MAMEKEY(F9))
		PORT_BIT(0x0080, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("H")       PORT_CODE(KEYCODE_H)            PORT_CHAR('H')
		PORT_BIT(0x0020, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("CAPS")    PORT_CODE(KEYCODE_CAPSLOCK)
		PORT_BIT(0x0010, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("A")       PORT_CODE(KEYCODE_A)            PORT_CHAR('A')
		PORT_BIT(0x0008, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("S")       PORT_CODE(KEYCODE_S)            PORT_CHAR('S')
		PORT_BIT(0x0004, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("D")       PORT_CODE(KEYCODE_D)            PORT_CHAR('D')
		PORT_BIT(0x0002, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("F")       PORT_CODE(KEYCODE_F)            PORT_CHAR('F')
		PORT_BIT(0x0001, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("G")       PORT_CODE(KEYCODE_G)            PORT_CHAR('G')
	PORT_START("KO5")
		PORT_BIT(0x8000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME(UTF8_UP)   PORT_CODE(KEYCODE_UP)           PORT_CHAR(UCHAR_MAMEKEY(UP))
		PORT_BIT(0x4000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME(UTF8_LEFT) PORT_CODE(KEYCODE_LEFT)         PORT_CHAR(UCHAR_MAMEKEY(LEFT))
		PORT_BIT(0x2000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("MENU")    PORT_CODE(KEYCODE_F5)           PORT_CHAR(UCHAR_MAMEKEY(F5))
		PORT_BIT(0x0080, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("M")       PORT_CODE(KEYCODE_M)            PORT_CHAR('M')
		PORT_BIT(0x0020, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Z")       PORT_CODE(KEYCODE_Z)            PORT_CHAR('Z')
		PORT_BIT(0x0010, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("X")       PORT_CODE(KEYCODE_X)            PORT_CHAR('X')
		PORT_BIT(0x0008, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("C")       PORT_CODE(KEYCODE_C)            PORT_CHAR('C')
		PORT_BIT(0x0004, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("V")       PORT_CODE(KEYCODE_V)            PORT_CHAR('V')
		PORT_BIT(0x0002, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("B")       PORT_CODE(KEYCODE_B)            PORT_CHAR('B')
		PORT_BIT(0x0001, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("N")       PORT_CODE(KEYCODE_N)            PORT_CHAR('N')
	PORT_START("KO6")
		PORT_BIT(0x2000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("LCKEY")   PORT_CODE(KEYCODE_F1)           PORT_CHAR(UCHAR_MAMEKEY(F1))
		PORT_BIT(0x0080, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("CONTRAST") PORT_CODE(KEYCODE_F2)          PORT_CHAR(UCHAR_MAMEKEY(F2))
		PORT_BIT(0x0020, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME(")")       PORT_CODE(KEYCODE_PGDN)         PORT_CHAR(')')      PORT_CHAR(']')
		PORT_BIT(0x0010, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("STOP")    PORT_CODE(KEYCODE_F7)           PORT_CHAR(UCHAR_MAMEKEY(F7))
		PORT_BIT(0x0008, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("INS")     PORT_CODE(KEYCODE_INSERT)
		PORT_BIT(0x0004, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("NEW ALL") PORT_CODE(KEYCODE_ESC)          PORT_CHAR(27)
		PORT_BIT(0x0002, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("BS")      PORT_CODE(KEYCODE_BACKSPACE)    PORT_CHAR(8)
		PORT_BIT(0x0001, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("CLS")     PORT_CODE(KEYCODE_DEL)          PORT_CHAR(12)
	PORT_START("KO7")
		PORT_BIT(0x0080, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("^")       PORT_CODE(KEYCODE_3_PAD)        PORT_CHAR('^')
		PORT_BIT(0x0020, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("/")       PORT_CODE(KEYCODE_SLASH)        PORT_CHAR('/')      PORT_CHAR('}')
		PORT_BIT(0x0010, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("(")       PORT_CODE(KEYCODE_PGUP)         PORT_CHAR('(')      PORT_CHAR('[')
		PORT_BIT(0x0008, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("9")       PORT_CODE(KEYCODE_9)            PORT_CHAR('9')      PORT_CHAR('\'')
		PORT_BIT(0x0004, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("8")       PORT_CODE(KEYCODE_8)            PORT_CHAR('8')      PORT_CHAR('_')
		PORT_BIT(0x0002, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("7")       PORT_CODE(KEYCODE_7)            PORT_CHAR('7')      PORT_CHAR('@')
		PORT_BIT(0x0001, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("ENG")     PORT_CODE(KEYCODE_F6)           PORT_CHAR(UCHAR_MAMEKEY(F6))
	PORT_START("KO8")
		PORT_BIT(0x0080, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("I")       PORT_CODE(KEYCODE_I)            PORT_CHAR('I')
		PORT_BIT(0x0020, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("*")       PORT_CODE(KEYCODE_ASTERISK)     PORT_CHAR('*')      PORT_CHAR('{')
		PORT_BIT(0x0010, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("6")       PORT_CODE(KEYCODE_6)            PORT_CHAR('6')      PORT_CHAR('\\')
		PORT_BIT(0x0008, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("5")       PORT_CODE(KEYCODE_5)            PORT_CHAR('5')      PORT_CHAR('~')
		PORT_BIT(0x0004, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("4")       PORT_CODE(KEYCODE_4)            PORT_CHAR('4')      PORT_CHAR('|')
		PORT_BIT(0x0002, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("P")       PORT_CODE(KEYCODE_P)            PORT_CHAR('P')
		PORT_BIT(0x0001, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("O")       PORT_CODE(KEYCODE_O)            PORT_CHAR('O')
	PORT_START("KO9")
		PORT_BIT(0x0080, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("J")       PORT_CODE(KEYCODE_J)            PORT_CHAR('J')
		PORT_BIT(0x0020, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("+")       PORT_CODE(KEYCODE_PLUS_PAD)     PORT_CHAR('+')
		PORT_BIT(0x0010, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("3")       PORT_CODE(KEYCODE_3)            PORT_CHAR('3')
		PORT_BIT(0x0008, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("2")       PORT_CODE(KEYCODE_2)            PORT_CHAR('2')
		PORT_BIT(0x0004, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("1")       PORT_CODE(KEYCODE_1)            PORT_CHAR('1')
		PORT_BIT(0x0002, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("L")       PORT_CODE(KEYCODE_L)            PORT_CHAR('L')
		PORT_BIT(0x0001, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("K")       PORT_CODE(KEYCODE_K)            PORT_CHAR('K')
	PORT_START("KO10")
		PORT_BIT(0x0080, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("SPACE")   PORT_CODE(KEYCODE_SPACE)        PORT_CHAR(' ')
		PORT_BIT(0x0020, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("-")       PORT_CODE(KEYCODE_MINUS)        PORT_CHAR('-')
		PORT_BIT(0x0010, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("EXE")     PORT_CODE(KEYCODE_ENTER)
		PORT_BIT(0x0008, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("ANS")     PORT_CODE(KEYCODE_END)
		PORT_BIT(0x0004, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME(".")       PORT_CODE(KEYCODE_STOP)         PORT_CHAR('.')
		PORT_BIT(0x0002, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("0")       PORT_CODE(KEYCODE_0)            PORT_CHAR('0')
		PORT_BIT(0x0001, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("??")      PORT_CODE(KEYCODE_5_PAD)
	PORT_START("KO11")
		PORT_BIT(0x0040, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("SHIFT")   PORT_CODE(KEYCODE_LSHIFT)       PORT_CHAR(UCHAR_SHIFT_1)
	PORT_START("KO12")
		PORT_BIT(0x0040, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("White F") PORT_CODE(KEYCODE_LALT)

	//touchscreen
	PORT_START("POSX")
		PORT_BIT( 0xff, 0x00, IPT_MOUSE_X ) PORT_CROSSHAIR(X, 1, 0, 0) PORT_SENSITIVITY(20) PORT_KEYDELTA(0)
	PORT_START("POSY")
		PORT_BIT( 0xff, 0x00, IPT_MOUSE_Y ) PORT_CROSSHAIR(Y, 1, 0, 0) PORT_SENSITIVITY(120) PORT_KEYDELTA(0)
	PORT_START("TOUCH")
		PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(MOUSECODE_BUTTON1) PORT_NAME("Touchscreen")
INPUT_PORTS_END

static INPUT_PORTS_START( pb2000c )
	PORT_START("KO0")
	PORT_START("KO1")
		PORT_BIT(0x0080, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("BRK")     PORT_CODE(KEYCODE_F10)             PORT_CHAR(UCHAR_MAMEKEY(F10))
		PORT_BIT(0x0020, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("OFF")     PORT_CODE(KEYCODE_7_PAD)
	PORT_START("KO2")
		PORT_BIT(0x8000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("TAB")     PORT_CODE(KEYCODE_TAB)             PORT_CHAR(9)
		PORT_BIT(0x4000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("'")       PORT_CODE(KEYCODE_1_PAD)           PORT_CHAR(39) PORT_CHAR('!')
		PORT_BIT(0x0020, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("CAPS")    PORT_CODE(KEYCODE_CAPSLOCK)
		PORT_BIT(0x0010, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Z")       PORT_CODE(KEYCODE_Z)               PORT_CHAR('Z')
		PORT_BIT(0x0004, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("A")       PORT_CODE(KEYCODE_A)               PORT_CHAR('A')
		PORT_BIT(0x0002, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Q")       PORT_CODE(KEYCODE_Q)               PORT_CHAR('Q') PORT_CHAR('?')
		PORT_BIT(0x0001, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("W")       PORT_CODE(KEYCODE_W)               PORT_CHAR('W') PORT_CHAR('@')
	PORT_START("KO3")
		PORT_BIT(0x8000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("(")       PORT_CODE(KEYCODE_PGUP)            PORT_CHAR('(') PORT_CHAR('"')
		PORT_BIT(0x4000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME(")")       PORT_CODE(KEYCODE_PGDN)            PORT_CHAR(')') PORT_CHAR('#')
		PORT_BIT(0x1000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("M1")      PORT_CODE(KEYCODE_F1)              PORT_CHAR(UCHAR_MAMEKEY(F1))
		PORT_BIT(0x0020, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("X")       PORT_CODE(KEYCODE_X)               PORT_CHAR('X')
		PORT_BIT(0x0010, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("C")       PORT_CODE(KEYCODE_C)               PORT_CHAR('C')
		PORT_BIT(0x0008, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("S")       PORT_CODE(KEYCODE_S)               PORT_CHAR('S')
		PORT_BIT(0x0004, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("D")       PORT_CODE(KEYCODE_D)               PORT_CHAR('D')
		PORT_BIT(0x0002, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("E")       PORT_CODE(KEYCODE_E)               PORT_CHAR('E') PORT_CHAR('\\')
		PORT_BIT(0x0001, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("R")       PORT_CODE(KEYCODE_R)               PORT_CHAR('R') PORT_CHAR('_')
	PORT_START("KO4")
		PORT_BIT(0x8000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("[")       PORT_CODE(KEYCODE_OPENBRACE)       PORT_CHAR('[') PORT_CHAR('$')
		PORT_BIT(0x4000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("]")       PORT_CODE(KEYCODE_CLOSEBRACE)      PORT_CHAR(']') PORT_CHAR('%')
		PORT_BIT(0x1000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("M2")      PORT_CODE(KEYCODE_F2)              PORT_CHAR(UCHAR_MAMEKEY(F2))
		PORT_BIT(0x0020, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("V")       PORT_CODE(KEYCODE_V)               PORT_CHAR('V')
		PORT_BIT(0x0010, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("B")       PORT_CODE(KEYCODE_B)               PORT_CHAR('B')
		PORT_BIT(0x0008, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("F")       PORT_CODE(KEYCODE_F)               PORT_CHAR('F')
		PORT_BIT(0x0004, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("G")       PORT_CODE(KEYCODE_G)               PORT_CHAR('G')
		PORT_BIT(0x0002, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("T")       PORT_CODE(KEYCODE_T)               PORT_CHAR('T') PORT_CHAR('`')
		PORT_BIT(0x0001, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Y")       PORT_CODE(KEYCODE_Y)               PORT_CHAR('Y') PORT_CHAR('{')
	PORT_START("KO5")
		PORT_BIT(0x8000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("|")       PORT_CODE(KEYCODE_BACKSLASH)       PORT_CHAR('|') PORT_CHAR('&')
		PORT_BIT(0x4000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("MEMO")    PORT_CODE(KEYCODE_F11)             PORT_CHAR(UCHAR_MAMEKEY(F11))
		PORT_BIT(0x1000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("M3")      PORT_CODE(KEYCODE_F3)              PORT_CHAR(UCHAR_MAMEKEY(F3))
		PORT_BIT(0x0020, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("N")       PORT_CODE(KEYCODE_N)               PORT_CHAR('N')
		PORT_BIT(0x0010, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("M")       PORT_CODE(KEYCODE_M)               PORT_CHAR('M')
		PORT_BIT(0x0008, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("H")       PORT_CODE(KEYCODE_H)               PORT_CHAR('H')
		PORT_BIT(0x0004, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("J")       PORT_CODE(KEYCODE_J)               PORT_CHAR('J')
		PORT_BIT(0x0002, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("U")       PORT_CODE(KEYCODE_U)               PORT_CHAR('U') PORT_CHAR('}')
		PORT_BIT(0x0001, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("I")       PORT_CODE(KEYCODE_I)               PORT_CHAR('I') PORT_CHAR('~')
	PORT_START("KO6")
		PORT_BIT(0x8000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("IN")      PORT_CODE(KEYCODE_F6)              PORT_CHAR(UCHAR_MAMEKEY(F6))
		PORT_BIT(0x4000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("OUT")     PORT_CODE(KEYCODE_F7)              PORT_CHAR(UCHAR_MAMEKEY(F7))
		PORT_BIT(0x1000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("M4")      PORT_CODE(KEYCODE_F4)              PORT_CHAR(UCHAR_MAMEKEY(F4))
		PORT_BIT(0x0020, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME(",")       PORT_CODE(KEYCODE_COMMA)           PORT_CHAR(',')
		PORT_BIT(0x0010, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("SPACE")   PORT_CODE(KEYCODE_SPACE)           PORT_CHAR(' ')
		PORT_BIT(0x0008, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("K")       PORT_CODE(KEYCODE_K)               PORT_CHAR('K')
		PORT_BIT(0x0004, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("L")       PORT_CODE(KEYCODE_L)               PORT_CHAR('L')
		PORT_BIT(0x0002, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("O")       PORT_CODE(KEYCODE_O)               PORT_CHAR('O') PORT_CHAR('<')
		PORT_BIT(0x0001, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("P")       PORT_CODE(KEYCODE_P)               PORT_CHAR('P') PORT_CHAR('>')
	PORT_START("KO7")
		PORT_BIT(0x8000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("CALC")    PORT_CODE(KEYCODE_HOME)
		PORT_BIT(0x1000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("ETC")     PORT_CODE(KEYCODE_F12)             PORT_CHAR(UCHAR_MAMEKEY(F12))
		PORT_BIT(0x0040, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Red S")   PORT_CODE(KEYCODE_LSHIFT) // PORT_CHAR(UCHAR_SHIFT_1)  design of the computer breaks this
		PORT_BIT(0x0020, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("ANS")     PORT_CODE(KEYCODE_END)
		PORT_BIT(0x0008, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME(";")       PORT_CODE(KEYCODE_COLON)           PORT_CHAR(';')
		PORT_BIT(0x0004, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME(":")       PORT_CODE(KEYCODE_BACKSLASH)       PORT_CHAR(':')
		PORT_BIT(0x0002, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("=")       PORT_CODE(KEYCODE_EQUALS)          PORT_CHAR('=') PORT_CHAR('^')
	PORT_START("KO8")
		PORT_BIT(0x2000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("NEW ALL") PORT_CODE(KEYCODE_ESC)             PORT_CHAR(27)
		PORT_BIT(0x0080, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("7")       PORT_CODE(KEYCODE_7)               PORT_CHAR('7')
		PORT_BIT(0x0020, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("0")       PORT_CODE(KEYCODE_0)               PORT_CHAR('0')
		PORT_BIT(0x0008, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("1")       PORT_CODE(KEYCODE_1)               PORT_CHAR('1')
		PORT_BIT(0x0004, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("2")       PORT_CODE(KEYCODE_2)               PORT_CHAR('2')
		PORT_BIT(0x0002, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("4")       PORT_CODE(KEYCODE_4)               PORT_CHAR('4')
		PORT_BIT(0x0001, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("5")       PORT_CODE(KEYCODE_5)               PORT_CHAR('5')
	PORT_START("KO9")
		PORT_BIT(0x0080, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("9")       PORT_CODE(KEYCODE_9)               PORT_CHAR('9')
		PORT_BIT(0x0020, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME(".")       PORT_CODE(KEYCODE_STOP)            PORT_CHAR('.')
		PORT_BIT(0x0010, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("EXE")     PORT_CODE(KEYCODE_ENTER)           PORT_CHAR(13)
		PORT_BIT(0x0008, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("3")       PORT_CODE(KEYCODE_3)               PORT_CHAR('3')
		PORT_BIT(0x0004, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("+")       PORT_CODE(KEYCODE_PLUS_PAD)        PORT_CHAR('+')
		PORT_BIT(0x0002, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("6")       PORT_CODE(KEYCODE_6)               PORT_CHAR('6')
		PORT_BIT(0x0001, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("-")       PORT_CODE(KEYCODE_MINUS)           PORT_CHAR('-')
	PORT_START("KO10")
		PORT_BIT(0x8000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("DEL")     PORT_CODE(KEYCODE_F8)              PORT_CHAR(UCHAR_MAMEKEY(F8))
		PORT_BIT(0x4000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("MENU")    PORT_CODE(KEYCODE_F5)              PORT_CHAR(UCHAR_MAMEKEY(F5))
		PORT_BIT(0x0080, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("*")       PORT_CODE(KEYCODE_ASTERISK)        PORT_CHAR('*')
		PORT_BIT(0x0008, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("BS")      PORT_CODE(KEYCODE_BACKSPACE)       PORT_CHAR(8)
		PORT_BIT(0x0004, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("/")       PORT_CODE(KEYCODE_SLASH)           PORT_CHAR('/')
		PORT_BIT(0x0002, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("RIGHT")   PORT_CODE(KEYCODE_RIGHT)           PORT_CHAR(UCHAR_MAMEKEY(RIGHT))
		PORT_BIT(0x0001, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("CAL")     PORT_CODE(KEYCODE_F9)              PORT_CHAR(UCHAR_MAMEKEY(F9))
	PORT_START("KO11")
		PORT_BIT(0x8000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("INS")     PORT_CODE(KEYCODE_INSERT)
		PORT_BIT(0x4000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("UP")      PORT_CODE(KEYCODE_UP)              PORT_CHAR(UCHAR_MAMEKEY(UP))
		PORT_BIT(0x0080, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("8")       PORT_CODE(KEYCODE_8)               PORT_CHAR('8')
		PORT_BIT(0x0004, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("CLS")     PORT_CODE(KEYCODE_DEL)             PORT_CHAR(12)
		PORT_BIT(0x0002, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("LEFT")    PORT_CODE(KEYCODE_LEFT)            PORT_CHAR(UCHAR_MAMEKEY(LEFT))
		PORT_BIT(0x0001, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("DOWN")    PORT_CODE(KEYCODE_DOWN)            PORT_CHAR(UCHAR_MAMEKEY(DOWN))
	PORT_START("KO12")
INPUT_PORTS_END

void pb1000_state::pb1000_palette(palette_device &palette) const
{
	palette.set_pen_color(0, rgb_t(138, 146, 148));
	palette.set_pen_color(1, rgb_t(92, 83, 88));
}


static const gfx_layout pb1000_charlayout =
{
	8, 8,                   /* 8 x 8 characters */
	256,                    /* 256 characters */
	1,                      /* 1 bits per pixel */
	{ 0 },                  /* no bitplanes */
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8},
	{ 0, 1, 2, 3, 4, 5, 6, 7},
	8*8                     /* 8 bytes */
};

static GFXDECODE_START( gfx_pb1000 )
	GFXDECODE_ENTRY( "hd44352", 0x0000, pb1000_charlayout, 0, 1 )
GFXDECODE_END

void pb1000_state::gatearray_w(offs_t offset, uint16_t data)
{
	m_gatearray[offset] = data&0xff;

	if (m_gatearray[0] && m_card1 && m_card1_reg)
		membank("bank1")->set_base(m_card1_reg->base());
	else if (m_gatearray[1] && m_card2 && m_card2_reg)
		membank("bank1")->set_base(m_card2_reg->base());
	else
		membank("bank1")->set_base(m_rom_reg->base());
}

uint16_t pb1000_state::read_touchscreen(uint8_t line)
{
	uint8_t x = ioport("POSX")->read()/0x40;
	uint8_t y = ioport("POSY")->read()/0x40;

	if (ioport("TOUCH")->read())
	{
		if (x == line-7)
			return (0x1000<<y);
	}

	return 0x0000;
}


uint16_t pb1000_state::pb1000_kb_r()
{
	uint16_t data = 0;
	u8 kb = m_kb_matrix & 15;

	if (kb == 13)
	{
		//Read all the input lines
		for (u8 line = 1; line < 13; line++)
		{
			data |= m_io_keyboard[line]->read();
			data |= read_touchscreen(line);
		}
	}
	else
	if (kb < 13)
	{
		data = m_io_keyboard[kb]->read();
		data |= read_touchscreen(kb);
	}

	return data;
}

uint16_t pb1000_state::pb2000c_kb_r()
{
	uint16_t data = 0;
	u8 kb = m_kb_matrix & 15;

	if (kb == 13)
	{
		//Read all the input lines
		for (u8 line = 1; line < 12; line++)
			data |= m_io_keyboard[line]->read();
	}
	else
	if (kb < 12)
		data = m_io_keyboard[kb]->read();

	return data;
}

void pb1000_state::kb_matrix_w(uint8_t data)
{
	if (BIT(data, 7))
	{
		if (!BIT(m_kb_matrix, 7))
			m_kb_timer->adjust(attotime::never, 0, attotime::never);
	}
	else
	{
		if (BIT(m_kb_matrix, 6) != BIT(data, 6))
		{
			if (BIT(data, 6))
				m_kb_timer->adjust(attotime::from_hz(32), 0, attotime::from_hz(32));
			else
				m_kb_timer->adjust(attotime::from_hz(256), 0, attotime::from_hz(256));
		}
	}

	m_kb_matrix = data;
}

uint8_t pb1000_state::pb1000_port_r()
{
	//TODO
	return 0x00;
}

uint8_t pb1000_state::pb2000c_port_r()
{
	//TODO
	return 0xfc;
}

void pb1000_state::port_w(uint8_t data)
{
	m_beeper->set_state((BIT(data,7) ^ BIT(data,6)));
	//printf("%x\n", data);
}


TIMER_CALLBACK_MEMBER(pb1000_state::keyboard_timer)
{
	m_maincpu->set_input_line(HD61700_KEY_INT, ASSERT_LINE);
	m_maincpu->set_input_line(HD61700_KEY_INT, CLEAR_LINE);
}

void pb1000_state::machine_start()
{
	std::string region_tag;
	m_rom_reg = memregion("rom");
	if (m_card1)
		m_card1_reg = memregion(region_tag.assign(m_card1->tag()).append(GENERIC_ROM_REGION_TAG).c_str());
	if (m_card2)
		m_card2_reg = memregion(region_tag.assign(m_card2->tag()).append(GENERIC_ROM_REGION_TAG).c_str());

	membank("bank1")->set_base(m_rom_reg->base());

	m_kb_timer = machine().scheduler().timer_alloc(timer_expired_delegate(FUNC(pb1000_state::keyboard_timer),this));
	m_kb_timer->adjust(attotime::from_hz(192), 0, attotime::from_hz(192));
}

void pb1000_state::pb1000(machine_config &config)
{
	/* basic machine hardware */
	HD61700(config, m_maincpu, 910000);
	m_maincpu->set_addrmap(AS_PROGRAM, &pb1000_state::pb1000_mem);
	m_maincpu->lcd_ctrl().set(m_hd44352, FUNC(hd44352_device::control_write));
	m_maincpu->lcd_read().set(m_hd44352, FUNC(hd44352_device::data_read));
	m_maincpu->lcd_write().set(m_hd44352, FUNC(hd44352_device::data_write));
	m_maincpu->kb_read().set(FUNC(pb1000_state::pb1000_kb_r));
	m_maincpu->kb_write().set(FUNC(pb1000_state::kb_matrix_w));
	m_maincpu->port_read().set(FUNC(pb1000_state::pb1000_port_r));
	m_maincpu->port_write().set(FUNC(pb1000_state::port_w));

	/* video hardware */
	screen_device &screen(SCREEN(config, "screen", SCREEN_TYPE_LCD));
	screen.set_refresh_hz(50);
	screen.set_vblank_time(ATTOSECONDS_IN_USEC(2500)); /* not accurate */
	screen.set_screen_update("hd44352", FUNC(hd44352_device::screen_update));
	screen.set_size(192, 32);
	screen.set_visarea(0, 192-1, 0, 32-1);
	screen.set_palette("palette");

	PALETTE(config, "palette", FUNC(pb1000_state::pb1000_palette), 2);
	GFXDECODE(config, "gfxdecode", "palette", gfx_pb1000);

	HD44352(config, m_hd44352, 910000);
	m_hd44352->on_cb().set_inputline("maincpu", HD61700_ON_INT);

	NVRAM(config, "nvram1", nvram_device::DEFAULT_ALL_0);
	NVRAM(config, "nvram2", nvram_device::DEFAULT_ALL_0);

	/* sound hardware */
	SPEAKER(config, "mono").front_center();
	BEEP(config, m_beeper, 3250).add_route(ALL_OUTPUTS, "mono", 1.00);
}

void pb1000_state::pb2000c(machine_config &config)
{
	pb1000(config);

	/* basic machine hardware */
	m_maincpu->set_addrmap(AS_PROGRAM, &pb1000_state::pb2000c_mem);
	m_maincpu->kb_read().set(FUNC(pb1000_state::pb2000c_kb_r));
	m_maincpu->port_read().set(FUNC(pb1000_state::pb2000c_port_r));

	GENERIC_CARTSLOT(config, m_card1, generic_plain_slot, "pb2000c_card");
	GENERIC_CARTSLOT(config, m_card2, generic_plain_slot, "pb2000c_card");

	/* Software lists */
	SOFTWARE_LIST(config, "card_list").set_original("pb2000c");
}

/* ROM definition */

ROM_START( pb1000 )
	ROM_REGION( 0x30000, "maincpu", ROMREGION_ERASEFF )
	ROM_LOAD( "hd61700.bin", 0x0000, 0x1800, CRC(b28c21a3) SHA1(be7ea62a15ff0c612f6efb2c95e6c2a11a738423))

	ROM_REGION( 0x10000, "rom", ROMREGION_ERASE )
	ROM_SYSTEM_BIOS(0, "basic", "BASIC")
	ROMX_LOAD( "pb1000.bin", 0x0000, 0x8000, CRC(8127a090) SHA1(067c1c2e7efb5249e95afa7805bb98543b30b630), ROM_BIOS(0) | ROM_SKIP(1))
	ROM_SYSTEM_BIOS(1, "basicj", "BASIC Jap")
	ROMX_LOAD( "pb1000j.bin", 0x0000, 0x8000, CRC(14a0df57) SHA1(ab47bb54eb2a24dcd9d2663462e9272d974fa7da), ROM_BIOS(1) | ROM_SKIP(1))

	ROM_REGION( 0x0800, "hd44352", 0 )
	ROM_LOAD( "charset.bin", 0x0000, 0x0800, CRC(7f144716) SHA1(a02f1ecc6dc0ac55b94f00931d8f5cb6b9ffb7b4))
ROM_END


ROM_START( pb2000c )
	ROM_REGION( 0x1800, "maincpu", ROMREGION_ERASEFF )
	ROM_LOAD( "hd61700.bin", 0x0000, 0x1800, CRC(25f9540c) SHA1(ecf98efadbdd4d1a74bc183eaf23f7113f2a12b1))

	ROM_REGION( 0x20000, "rom", ROMREGION_ERASE )
	ROMX_LOAD( "pb2000c.bin", 0x0000, 0x10000, CRC(41933631) SHA1(70b654dc375b647afa042baf8b0a139e7fa604e8), ROM_SKIP(1))

	ROM_REGION( 0x0800, "hd44352", 0 )
	ROM_LOAD( "charset.bin", 0x0000, 0x0800, CRC(7f144716) SHA1(a02f1ecc6dc0ac55b94f00931d8f5cb6b9ffb7b4))
ROM_END


ROM_START( ai1000 )
	ROM_REGION( 0x1800, "maincpu", ROMREGION_ERASEFF )
	ROM_LOAD( "hd61700.bin", 0x0000, 0x1800, CRC(25f9540c) SHA1(ecf98efadbdd4d1a74bc183eaf23f7113f2a12b1))

	ROM_REGION( 0x20000, "rom", ROMREGION_ERASE )
	ROMX_LOAD( "ai1000.bin", 0x0000, 0x10000, CRC(72aa3ee3) SHA1(ed1d0bc470902ea73bc4588147a589b1793afb40), ROM_SKIP(1))

	ROM_REGION( 0x0800, "hd44352", 0 )
	ROM_LOAD( "charset.bin", 0x0000, 0x0800, CRC(7f144716) SHA1(a02f1ecc6dc0ac55b94f00931d8f5cb6b9ffb7b4))
ROM_END

/* Driver */

/*    YEAR  NAME     PARENT   COMPAT  MACHINE  INPUT    CLASS         INIT        COMPANY  FULLNAME    FLAGS */
COMP( 1987, pb1000,  0,       0,      pb1000,  pb1000,  pb1000_state, empty_init, "Casio", "PB-1000",  MACHINE_NOT_WORKING)
COMP( 1989, pb2000c, 0,       0,      pb2000c, pb2000c, pb1000_state, empty_init, "Casio", "PB-2000c", MACHINE_NOT_WORKING)
COMP( 1989, ai1000,  pb2000c, 0,      pb2000c, pb2000c, pb1000_state, empty_init, "Casio", "AI-1000",  MACHINE_NOT_WORKING)
