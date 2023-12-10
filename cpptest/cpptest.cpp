#include <SFML/Graphics.hpp>
#include <Vector>
#include <iostream>
#include <thread>
#include <chrono>

using namespace std;

//64 cols, 32 rows grid[64][32], displays 8 x 15 sprites
//on will be green, off will be black
struct Display {
	uint8_t cols = 64;
	uint8_t rows = 32;
	uint8_t pixelsScale = 12;
	//multi dimensional arrays of rectangles that will be drawn as scaled pixels
	sf::RenderWindow window;
	sf::RectangleShape pixels[64][32];
	uint8_t collisionPixels[64][32] = {};
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
	std::vector<uint8_t> stack;

	//delay timer decrements at a rate of 60 times per second until it reaches 0
	uint8_t delayTimer;

	//behaves like the delayTimer but emits sound as long as it's not 0
	uint8_t soundTimer;

	//general purpose variable register
	uint8_t V[0x10];

	Chip8() 
	{
		//initScreen();
		//ClearIn();
		//returnSub();
		//gotoNNN();
		//callNNN();
		//sixXNN();
		//sevXNN();
		DXYN();
	}

	void callNNN() {
		memory[512] = 0x01;
		memory[513] = 0xAB;
	}

	void returnSub() {
		memory[512] = 0x00;
		memory[513] = 0xEE;
	}

	void ClearIn() {
		memory[512] = 0x00;
		memory[513] = 0xE0;
	}

	void gotoNNN() {
		memory[512] = 0x11;
		memory[513] = 0x23;
	}

	void sixXNN()
	{
		memory[512] = 0x6F;
		memory[513] = 0xAB;
	}

	void sevXNN()
	{
		memory[512] = 0x7F;
		memory[513] = 0xAB;
	}

	void DXYN()
	{
		memory[512] = 0xDF;
		memory[513] = 0xAB;
	}

	void windowEvent() 
	{
		while (screen.window.isOpen())
		{
			// check all the window's events that were triggered since the last iteration of the loop
			sf::Event event;
			while (screen.window.pollEvent(event))
			{
				// "close requested" event: we close the window
				if (event.type == sf::Event::Closed)
					screen.window.close();
			}
		}
	}

	void initScreen() 
	{
		screen.window.create(sf::VideoMode(screen.cols * screen.pixelsScale, screen.rows * screen.pixelsScale), "Chip-8 Emulator", sf::Style::Titlebar | sf::Style::Close);
		screen.window.setFramerateLimit(30);

		//set the rectangle prooperties;
		for (int x = 0; x < screen.cols; ++x) {
			for (int y = 0; y < screen.rows; ++y) {
				screen.pixels[x][y].setSize(sf::Vector2f(screen.pixelsScale, screen.pixelsScale));
				screen.pixels[x][y].setPosition(x * screen.pixelsScale, y * screen.pixelsScale);
				screen.pixels[x][y].setFillColor(sf::Color::Green);
			}
		}

		screen.window.display();
	}

	void renderPixels() 
	{
		// Render each rectangle pixel
		for (int x = 0; x < screen.cols; x = x + 2) {
			for (int y = 0; y < screen.rows; ++y) {
				screen.window.draw(screen.pixels[x][y]);
			}
		}
		screen.window.display();
	}

	void drawSprite(uint8_t x, uint8_t y, uint8_t n)
	{
		// x and y are the values stored in V[x], V[y], 0 - 255, n a nibble 0 - 15
		//if there was at least one collision, set VF to 1
		uint8_t xCoord = x % 64;
		uint8_t yCoord = y % 32;

		//save state of screen
		//compare after xoring with the sprite?

		sf::RectangleShape;
		
		//set VF to 0
		V[0xF] = 1;

		//for n rows, 
		for (int i = 0; i < n; i++)
		{
			uint8_t mByte = memory[registerI++];
			
			for (int j = 0; j < 8; j++)
			{

			}

			//I need to modify screen.pixels 2d array

			screen.pixels[x][y].setPosition(x * screen.pixelsScale, y * screen.pixelsScale); // use to set coordinate?
		}

		V[15] = 1;

	}

	void fetch() 
	{
		uint8_t byte1 = memory[PC];
		uint8_t byte2 = memory[PC + 1];

		uint16_t cInstruction = (static_cast<uint16_t>(byte1) << 8) | byte2;;
		PC += 2;

		cout << "cInstru: " << hex << cInstruction << endl;
		decode(cInstruction);
	}

	void decode(uint16_t cInstru) 
	{
		uint16_t fNibble = (cInstru & 0xF000) >> 4;

		cout << "nibble: " << hex << fNibble << endl;

		switch (fNibble) {
			case 0x0: 
			{
				uint16_t argument = cInstru & 0x0FFF;

				switch (argument)
				{
					case 0x00E0: 
					{
						cout << "Clear the screen" << endl;
						break;
					}
					case 0x00EE: 
					{
						cout << "Return from subroutine" << endl;
						break;
					}
					default:
					{
						cout << "Call machine code routine" << endl;
						break;
					}
				}
			}
			case 0x100:
			{
				cout << " 1NNN instruction; go to NNN" << endl;
				PC = (cInstru & 0x0FFF);
				break;
			}
			case 0x600:
			{
				cout << "6XNN instruction" << endl;
				uint8_t X = (cInstru & 0x0F00) >> 8;
				uint8_t NN = (cInstru & 0x00FF);
				V[X] = NN;
				break;
			}
			case 0x700:
			{
				cout << "7XNN instruction" << endl;
				uint8_t X = (cInstru & 0x0F00) >> 8;
				uint8_t NN = (cInstru & 0x00FF);
				V[X] = V[X] + NN;
			}
			case 0xA00:
			{
				cout << "ANNN instruction set index register i" << endl;
				uint16_t NNN = cInstru & 0x0FFF;
				registerI = NNN;
				break;
			}
			case 0xD00:
			{
				/*
				each sprite has a width of 8 pixels and a variable height
				each pixel can be represent by 1 bit in the 8 pixel byte
				if the bit is 0, it is off, leave as it is; black color
				if bit is 1, toggle against the current state; bit = !bit;
				if bit is toggled from on to off, flag register gets set to 1 
				otherwise it will be set to 0, this is a collision

				*/

				cout << "DXYN draw instruction" << endl;
				uint8_t XIndex = (cInstru & 0x0F00) >> 8;
				uint8_t YIndex = (cInstru & 0x00F0) >> 4;
				uint8_t NPixelRows = (cInstru & 0x000F);

				//read N bytes from memory starting at registerI. from I to N 8 bits wide, N bits tall, 8 x N sprite stored
				// X and Y are the offset, where the sprite is being drawn from, location
				
				// v registers of Byte 0 - 15;
				uint8_t x = V[XIndex];
				uint8_t y = V[YIndex];

				drawSprite(x, y, NPixelRows);

				//XOR sprite against current framebuffer, if any pixels are erased, then set VF to 1, otherwise VF to 0
				//XOR against current pixels, that toggles, 
				//1010 ^ 0010 = we flipped the first bit 1000 | 1010 = 1010
				//
				break;
			}
			default:
			{
				cout << "can't interpret opcode" << endl;
				break;
			}
		}
	}

	void execute() 
	{

	}

	void clearDisplay() 
	{
		screen.window.clear(sf::Color::Black);
		screen.window.display();
	}
};



int main()
{
	Chip8 chip;

	chip.fetch();	
	
	/*chip.renderPixels();

	chip.windowEvent();*/

	cin.get();

	return 0;
}



