#include <SFML/Graphics.hpp>
#include <Vector>
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>

using namespace std;

struct Display {
	const uint8_t cols = 64;
	const uint8_t rows = 32;
	const uint8_t pixelsScale = 12;

	sf::RenderWindow window;
	//2d array will be drawn as upscaled pixels
	uint8_t screenGrid[64][32] = {};
};

class Chip8 {

public:
	//memory
	uint8_t memory[0x1000];

	//64 x 32 pixel monochrome display; black or white
	Display screen;

	//program counter, points at the current instruction in memory
	uint16_t PC = 0x200;

	//index register to point at locations in memory
	uint16_t registerI;

	//store addresses the interpreter should return to after subroutines have finished
	std::vector<uint16_t> stack;

	//delay timer decrements at a rate of 60 times per second until it reaches 0
	uint8_t delayTimer;

	//behaves like the delayTimer but emits sound as long as it's not 0
	uint8_t soundTimer;

	//general purpose variable register
	uint8_t V[0x10];

	//keyboard input 16 keys 0-F
	uint8_t Keys[16];

	//mute instructions for debug
	bool mute = true;

	uint8_t fonts[80] = {
	   0xF0, 0x90, 0x90, 0x90, 0xF0,
	   0x20, 0x60, 0x20, 0x20, 0x70,
	   0xF0, 0x10, 0xF0, 0x80, 0xF0,
	   0xF0, 0x10, 0xF0, 0x10, 0xF0,
	   0x90, 0x90, 0xF0, 0x10, 0x10,
	   0xF0, 0x80, 0xF0, 0x10, 0xF0,
	   0xF0, 0x80, 0xF0, 0x90, 0xF0,
	   0xF0, 0x10, 0x20, 0x40, 0x40,
	   0xF0, 0x90, 0xF0, 0x90, 0xF0,
	   0xF0, 0x90, 0xF0, 0x10, 0xF0,
	   0xF0, 0x90, 0xF0, 0x90, 0x90,
	   0xE0, 0x90, 0xE0, 0x90, 0xE0,
	   0xF0, 0x80, 0x80, 0x80, 0xF0,
	   0xE0, 0x90, 0x90, 0x90, 0xE0,
	   0xF0, 0x80, 0xF0, 0x80, 0xF0,
	   0xF0, 0x80, 0xF0, 0x80, 0x80 };

	Chip8() 
	{
		initScreen();
	}

	void windowEvent() 
	{
		while (screen.window.isOpen())
		{
			sf::Event event;
			while (screen.window.pollEvent(event))
			{
				if (event.type == sf::Event::Closed)
					screen.window.close();
			}
		}
	}

	void initScreen() 
	{
		screen.window.create(sf::VideoMode(screen.cols * screen.pixelsScale, screen.rows * screen.pixelsScale), "Chip-8 Emulator", sf::Style::Titlebar | sf::Style::Close);
		screen.window.setFramerateLimit(60);
		screen.window.display();
	}

	void renderPixels() 
	{
		screen.window.clear(sf::Color::Black);

		for (int x = 0; x < screen.cols; ++x)
		{
			for (int y = 0; y < screen.rows; ++y)
			{
				if (screen.screenGrid[x][y] == 1)
				{
					sf::RectangleShape pixel(sf::Vector2f(screen.pixelsScale, screen.pixelsScale));
					pixel.setPosition(x * screen.pixelsScale, y * screen.pixelsScale);
					pixel.setFillColor(sf::Color::Green);

					/*sf::RectangleShape inner(sf::Vector2f(10, 10));
					inner.setPosition(x * screen.pixelsScale, y * screen.pixelsScale);
					inner.setFillColor(sf::Color::Green);*/

					screen.window.draw(pixel);
					//screen.window.draw(inner);
				}
			}
		}
		screen.window.display();
	}

	void clearScreen()
	{
		for (int x = 0; x < screen.cols; ++x)
		{
			for (int y = 0; y < screen.rows; ++y)
			{
				screen.screenGrid[x][y] = 0;
			}
		}
		screen.window.clear(sf::Color::Black);
		screen.window.display();
	}

	void drawSprite(uint8_t x, uint8_t y, uint8_t n)
	{
		// x and y are the values stored in V[x], V[y], 0 - 255, n a nibble 0 - 15
		//if there was at least one collision, set VF to 1
		uint8_t xCoord = x % screen.cols;
		uint8_t yCoord = y % screen.rows;

		//set VF to 0
		V[0xF] = 0;

		//for n rows, 
		for (int row = 0; row < n; row++)
		{
			uint8_t mByte = memory[registerI + row];
			
			for (int col = 0; col < 8; col++)
			{
				//in here we need to grab the bits from Mbyte which represent each pixel and whether they are on or off
				uint8_t bytePixel = ((mByte & 0x80 >> col) != 0) ? 1 : 0;
				uint8_t collision = (screen.screenGrid[col + xCoord][row + yCoord] ^ bytePixel == 1) ? 1 : 0;
				if (collision)
				{
					V[15] = collision;
				}
				screen.screenGrid[col + xCoord][row + yCoord] = bytePixel;
			}
		}
		renderPixels();
	}

	void fetch() 
	{
		uint8_t byte1 = memory[PC];
		uint8_t byte2 = memory[PC + 1];

		uint16_t cInstruction = (static_cast<uint16_t>(byte1) << 8) | byte2;;
		PC += 2;

		if (!mute) cout << "cInstru: " << hex << cInstruction << endl;
		decode(cInstruction);
	}

	void decode(uint16_t cInstru)
	{
		uint16_t fNibble = (cInstru & 0xF000) >> 4;

		if (!mute) cout << "nibble: " << hex << fNibble << endl;

		switch (fNibble) {
			case 0x0: 
			{
				uint16_t argument = cInstru & 0x00FF;

				switch (argument)
				{
					case 0xE0: 
					{
						if (!mute) cout << "Clear the screen" << endl;
						clearScreen();
						break;
					}
					case 0xEE: 
					{
						if (!mute) cout << "Return from subroutine" << endl;
						PC = stack.back();
						stack.pop_back();
						break;
					}
					default:
					{
						if (!mute) cout << "Call machine code routine" << endl;
						break;
					}
				}
				break;
			}
			case 0x100:
			{
				if (!mute) cout << "1NNN instruction; go to NNN" << endl;
				PC = (cInstru & 0x0FFF);
				break;
			}
			case 0x200:
			{
				if (!mute) cout << "2NNN instruction; Calls subroutine at NNN" << endl;
				stack.push_back(PC);
				PC = (cInstru & 0x0FFF);
				break;
			}
			case 0x300:
			{
				if (!mute) cout << "3NNN instruction; skin the next instruction if VX == NN" << endl;
				uint8_t X = (cInstru & 0x0F00) >> 8;
				uint8_t NN = (cInstru & 0x00FF);

				if (V[X] == NN)
				{
					PC += 2;
				}
				break;
			}
			case 0x400:
			{
				if (!mute) cout << "4NNN instruction; skin the next instruction VX != NN" << endl;
				uint8_t X = (cInstru & 0x0F00) >> 8;
				uint8_t NN = (cInstru & 0x00FF);

				if (V[X] != NN)
				{
					PC += 2;
				}
				break;
			}
			case 0x500:
			{
				if (!mute) cout << "5XY0 instruction; skin the next instruction VX != NN" << endl;
				uint8_t X = (cInstru & 0x0F00) >> 8;
				uint8_t Y = (cInstru & 0x00F0) >> 4;

				if (V[X] == V[Y])
				{
					PC += 2;
				}
				break;
			}
			case 0x600:
			{
				if (!mute) cout << "6XNN instruction" << endl;
				uint8_t X = (cInstru & 0x0F00) >> 8;
				uint8_t NN = (cInstru & 0x00FF);
				V[X] = NN;
				break;
			}
			case 0x700:
			{
				if (!mute) cout << "7XNN instruction" << endl;
				uint8_t X = (cInstru & 0x0F00) >> 8;
				uint8_t NN = (cInstru & 0x00FF);
				V[X] = V[X] + NN;
				break;
			}
			case 0x800:
			{
				if (!mute) cout << "8XYN instruction" << endl;
				uint8_t X = (cInstru & 0x0F00) >> 8;
				uint8_t Y = (cInstru & 0x00F0) >> 4;
				uint16_t lastNibble = cInstru & 0x000F;

				switch (lastNibble)
				{
					case 0x0:
					{
						V[X] = V[Y];
						break;
					}
					case 0x1:
					{
						V[X] = V[X] | V[Y];
						break;
					}
					case 0x2:
					{
						V[X] = V[X] & V[Y];
						break;
					}
					case 0x3:
					{
						V[X] = V[X] ^ V[Y];
						break;
					}
					case 0x4:
					{
						V[X] = V[X] + V[Y];
						(V[X] + V[Y] > 0xFF) ? V[0xF] = 1 : V[0xF] = 0;
						break;
					}
					case 0x5:
					{
						V[X] = V[X] - V[Y];
						//there is no borrow if the number you are substracting is bigger than the number you are substacting from.
						(V[X] > V[Y]) ? V[0xF] = 1 : V[0xF] = 0;
						break;
					}
					case 0x6:
					{
						V[0xF] = V[X] & (0x80 >> 7);
						V[X] = V[X] >> 1;
						break;
					}
					case 0x7:
					{
						V[X] = V[Y] - V[X];
						(V[Y] > V[X]) ? V[0xF] = 1 : V[0xF] = 0;
						break;
					}
					case 0xE:
					{
						V[0xF] = V[X] & 0x80;
						V[X] = V[X] << 1;
						break;
					}
				}
				break;
			}
			case 0x900:
			{
				if (!mute) cout << "9XY0 instruction" << endl;
				uint8_t X = (cInstru & 0x0F00) >> 8;
				uint8_t Y = (cInstru & 0x00F0) >> 4;
				if (V[X] != V[Y])
				{
					PC += 2;
				}
				break;
			}
			case 0xA00:
			{
				if (!mute) cout << "ANNN instruction set index register i" << endl;
				uint16_t NNN = cInstru & 0x0FFF;
				registerI = NNN;
				break;
			}
			case 0xB00:
			{
				if (!mute) cout << "BNNN instruction" << endl;
				uint16_t NNN = cInstru & 0x0FFF;
				PC = NNN + V[0x0];
				break;
			}
			case 0xC00:
			{
				if (!mute) cout << "CXNN instruction" << endl;
				uint8_t X = (cInstru & 0x0F00) >> 8;
				uint8_t NN = (cInstru & 0x00FF);
				V[X] = (rand() % 255) & NN;
				break;
			}
			case 0xD00:
			{
				cout << "DXYN draw instruction" << endl;
				uint8_t X = (cInstru & 0x0F00) >> 8;
				uint8_t Y = (cInstru & 0x00F0) >> 4;
				uint8_t N = (cInstru & 0x000F);

				//read N bytes from memory starting at registerI. from I to N 8 bits wide, N bits tall, 8 x N sprite stored
				// X and Y are the offset, where the sprite is being drawn from, location
				
				uint8_t vx = V[X];
				uint8_t vy = V[Y];

				drawSprite(vx, vy, N);
				break;
			}
			case 0xE00:
			{
				if (!mute) cout << "EX instructions" << endl;
				uint8_t X = (cInstru & 0x0F00) >> 8;
				uint8_t argument = cInstru & 0x00FF;

				switch (argument)
				{
					case 0x9E:
					{
						cout << "keyop EX9E" << endl;
						if (Keys[V[X]] == 1)
						{
							PC += 2;
						}
						break;
					}
					case 0xA1:
					{
						cout << "keyop EXA1" << endl;
						if (Keys[V[X]] != 1)
						{
							PC += 2;
						}
						break;
					}
				}
				break;
			}
			case 0xF00: 
			{
				if (!mute) cout << "FX instructions" << endl;
				uint8_t X = (cInstru & 0x0F00) >> 8;
				uint8_t argument = cInstru & 0x00FF;

				switch (argument)
				{
					case 0x7:
					{
						V[X] = delayTimer;
						break;
					}
					case 0xA:
					{
						uint8_t k = waitInput();
						V[X] = Keys[k];
						break;
					}
					case 0x15:
					{
						delayTimer = V[X];
						break;
					}
					case 0x18:
					{
						soundTimer = V[X];
						break;
					}
					case 0x1E:
					{
						registerI = registerI + V[X];
						break;
					}
					case 0x29:
					{
						registerI = memory[V[X]];
						break;
					}
					case 0x33:
					{
						uint8_t bcd = V[X];

						memory[registerI] = bcd / 100;
						memory[registerI + 1] = (bcd / 10) % 10;
						memory[registerI + 2] = bcd % 10;
						break;
					}
					case 0x55:
					{

						for (int i = 0; i <= X; i++)
						{
							memory[registerI + i] = V[i];
						}
						break;
					}
					case 0x65:
					{
						for (int i = 0; i <= X; i++)
						{
							V[i] = memory[registerI + i];
						}
						break;
					}
				}
				break;
			}
			default:
			{
				cout << "can't interpret opcode" << endl;
				break;
			}
		}
	}

	uint8_t waitInput()
	{
		bool keyFound = 0;
		while (true)
		{
			for (int i = 0; i <= 15; i++)
			{
				if (Keys[i] == 1)
				{
					keyFound = i;
					break;
				}
			}
			if (keyFound > 0) return keyFound;
		}
	}

	void getInput(sf::Keyboard::Key kPressed)
	{
		memset(Keys, 0, sizeof(Keys));

		switch (kPressed)
		{
			case sf::Keyboard::Num1:
			{
				cout << "num 1 key pressed" << endl;
				Keys[0x0] = 1;
				break;
			}
			case sf::Keyboard::Key::Num2:
			{
				cout << "num 2 key pressed" << endl;
				Keys[0x1] = 1;
				break;
			}
			case sf::Keyboard::Key::Num3:
			{
				cout << "num 3 key pressed"  << endl;
				Keys[0x2] = 1;
				break;
			}
			case sf::Keyboard::Key::Num4:
			{
				cout << "num 4 key pressed" << endl;
				Keys[0x3] = 1;
				break;
			}
			case sf::Keyboard::Key::Q:
			{
				cout << "Q key pressed" << endl;
				Keys[0x4] = 1;
				break;
			}
			case sf::Keyboard::Key::W:
			{
				cout << "W key pressed" << endl;
				Keys[0x5] = 1;
				break;
			}
			case sf::Keyboard::Key::E:
			{
				cout << "E key pressed" << endl;
				Keys[0x6] = 1;
				break;
			}
			case sf::Keyboard::Key::R:
			{
				cout << "R key pressed" << endl;
				Keys[0x7] = 1;
				break;
			}
			case sf::Keyboard::Key::A:
			{
				cout << "A key pressed" << endl;
				Keys[0x8] = 1;
				break;
			}
			case sf::Keyboard::Key::S:
			{
				cout << "S key pressed" << endl;
				Keys[0x9] = 1;
				break;
			}
			case sf::Keyboard::Key::D:
			{
				cout << "D key pressed" << endl;
				Keys[0xA] = 1;
				break;
			}
			case sf::Keyboard::Key::F:
			{
				cout << "F key pressed" << endl;
				Keys[0xB] = 1;
				break;
			}
			case sf::Keyboard::Key::Z:
			{
				cout << "Z key pressed" << endl;
				Keys[0xC] = 1;
				break;
			}
			case sf::Keyboard::Key::X:
			{
				cout << "X key pressed" << endl;
				Keys[0xD] = 1;
				break;
			}
			case sf::Keyboard::Key::C:
			{
				cout << "C key pressed" << endl;
				Keys[0xE] = 1;
				break;
			}
			case sf::Keyboard::Key::V:
			{
				cout << "V key pressed" << endl;
				Keys[0xF] = 1;
				break;
			}
			default:
			{
				cout << "unknown key";
				break;
			}
		}
	}

	void resetState()
	{
		registerI = 0;
		PC = 0x200;
		memset(V, 0, sizeof(V));

		memcpy(&memory[0x50], &fonts, sizeof(fonts));

		string ibmLogo = "IBGM Logo.ch8";
		string testOpcode = "test_opcode.ch8";
		string pong = "Pong.ch8";
		string invader = "invaders.c8";

		ifstream in;
		in.open(invader, ios::binary | ios::in | ios::ate);

		if (in.is_open())
		{
			cout << "file is opened" << endl;
			in.seekg(0, std::ios_base::end);
			auto length = in.tellg();
			in.seekg(0, std::ios_base::beg);
			in.read(reinterpret_cast<char*>(&memory[512]), length);
			in.close();
		}
	}
};


int main()
{
	Chip8 chip;
	chip.resetState();

	while (chip.screen.window.isOpen())
	{
		chip.fetch();

		// Check for window events
		sf::Event event;
		while (chip.screen.window.pollEvent(event))
		{
			if (event.type == sf::Event::Closed)
				chip.screen.window.close();

			if (event.type == sf::Event::KeyPressed)
			{
				chip.getInput(event.key.code);
			}
		}

		// Update timers
		if (chip.delayTimer > 0)
		{
			chip.delayTimer--;
		}
	}
	cin.get();

	return 0;
}



