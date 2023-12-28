#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <Vector>
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <cmath>

using namespace std;

constexpr auto PI = 3.14159265358979323846;
constexpr auto twoPI = 6.28318530718f;

struct Display {
	const uint8_t cols = 64;
	const uint8_t rows = 32;
	const uint8_t pixelsScale = 12;

	sf::RenderWindow window;
	//2d array will be drawn as upscaled pixels
	uint8_t screenGrid[64][32] = {};
};

struct Audio
{
	int sampleRate = 44100;
	double frequency = 220.0f;
	double duration = 3.0;
	sf::Sound sound;
	sf::SoundBuffer soundBuffer;
	std::vector<int16_t> squareWaveBard;
};


class Chip8 {

public:
	//memory
	uint8_t memory[0x1000];

	//64 x 32 pixel monochrome display; black or white
	Display screen;

	Audio audio; 

	bool draw = true;

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
	bool mute = false;

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


	void initScreen() 
	{
		screen.window.create(sf::VideoMode(screen.cols * screen.pixelsScale, screen.rows * screen.pixelsScale), "Chip-8 Emulator", sf::Style::Titlebar | sf::Style::Close);
	}

	void renderPixels()
	{
		screen.window.clear(sf::Color::Black);

		sf::RectangleShape pixel(sf::Vector2f(screen.pixelsScale, screen.pixelsScale));
		pixel.setFillColor(sf::Color::Green);

		for (int x = 0; x < screen.cols; ++x)
		{
			for (int y = 0; y < screen.rows; ++y)
			{
				if (screen.screenGrid[x][y] == 1)
				{
					pixel.setPosition(x * screen.pixelsScale, y * screen.pixelsScale);
					screen.window.draw(pixel);
				}
			}
		}
		screen.window.display();
		draw = false;
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
	}
	
	void initAudio()
	{
		audio.sound.setBuffer(audio.soundBuffer);
		audio.sound.setLoop(true);
		audio.sound.setVolume(10.0f);
		audio.squareWaveBard = generateSquareWaveBard(audio.sampleRate, audio.frequency, audio.duration);

		if (!audio.soundBuffer.loadFromSamples(audio.squareWaveBard.data(), audio.sampleRate, 1, audio.sampleRate))
		{
			throw "Error loading sound sample";
		}
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
				uint8_t currentPixel = screen.screenGrid[col + xCoord][row + yCoord];

				if (currentPixel && bytePixel)
				{
					V[0xF] = 1;
				}
				screen.screenGrid[col + xCoord][row + yCoord] = (screen.screenGrid[col + xCoord][row + yCoord]) ^ bytePixel;
			}
		}
		draw = true;
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
						draw = true;
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
				if (!mute) cout << "DXYN draw instruction" << endl;
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
						if (!mute) cout << "keyop EX9E" << endl;
						if (Keys[V[X]] == 1)
						{
							PC += 2;
						}
						break;
					}
					case 0xA1:
					{
						if (!mute) cout << "keyop EXA1" << endl;
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
						cout << "OxA instruction" << endl;
						bool kPressed = false;

						for (uint8_t i = 0; i < 16; i++)
						{
							if (Keys[i] == 1)
							{
								kPressed = true;
								V[X] = i & 0x00FF;
								break;
							}
						}

						if (!kPressed)
						{
							PC -= 2;
						}
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
						registerI = memory[V[X]] * 5;
						break;
					}
					case 0x33:
					{
						uint8_t bcd = V[X];

						memory[registerI] = bcd / 100;
						memory[registerI + 1] = (bcd % 100)/10;
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
				if(!mute) cout << "can't interpret opcode" << endl;
				break;
			}
		}
	}

	std::vector<int16_t> generateSquareWaveTwo(double frequency, double duration, double amplitude, int sampleRate)
	{
		int numSamples = static_cast<int>(duration * sampleRate);
		std::vector<int16_t> squareWave(numSamples);

		for (int i = 0; i < numSamples; ++i)
		{
			int16_t  t = static_cast<double>(i) / sampleRate;
			squareWave[i] = amplitude * std::sin(2.0 * PI * frequency * t) > 0.0 ? amplitude : -amplitude;
		}

		return squareWave;
	}

	std::vector<int16_t> generateSquareWaveBard(int sampleRate, double frequency, double duration, double amplitude = 32767)
	{
		// Calculate parameters
		const double period = 1.0 / frequency;
		const int numSamples = static_cast<int>(sampleRate * duration);
		amplitude = amplitude * .8f;

		// Initialize empty vector
		std::vector<int16_t> samples(numSamples);

		// Generate square wave samples
		for (int i = 0; i < numSamples; ++i)
		{
			double phase = fmod(2.0 * twoPI * i / sampleRate, period);
			samples[i] = static_cast<int16_t>(amplitude * (phase < period / 2 ? 1.0 : -1.0));
		}

		return samples;
	}

	std::vector<std::int16_t> generateSquareWave(float soundFrequency, std::uint32_t sampleRate)
	{
		
		std::vector<std::int16_t> rawSound(sampleRate);
		float amp = 0.9f;

		for (std::size_t i = 0; i < rawSound.size(); i++)
		{
			float time = static_cast<float>(i);
			std::int16_t out = 0;
			std::int16_t amplitude = static_cast<std::int16_t>(32767.f * amp);
			std::int32_t tpc = static_cast<std::int32_t>(sampleRate / soundFrequency);
			std::int32_t cyclepart = static_cast<std::int32_t>(time) % tpc;
			std::int32_t halfcycle = tpc / 2;

			if (cyclepart < halfcycle)
			{
				out = amplitude;
			}

			rawSound.at(i) = out;
		}

		return rawSound;
	}

	void playSound()
	{
		cout << "playing sound" << endl;
	}

	void getInput(sf::Keyboard::Key kPressed)
	{
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

	void releaseInput(sf::Keyboard::Key kReleased)
	{
		switch (kReleased)
		{
		case sf::Keyboard::Num1:
			cout << "num 1 key released" << endl;
			Keys[0x0] = 0;
			break;

		case sf::Keyboard::Num2:
			cout << "num 2 key released" << endl;
			Keys[0x1] = 0;
			break;

		case sf::Keyboard::Num3:
			cout << "num 3 key released" << endl;
			Keys[0x2] = 0;
			break;

		case sf::Keyboard::Num4:
			cout << "num 4 key released" << endl;
			Keys[0x3] = 0;
			break;

		case sf::Keyboard::Q:
			cout << "Q key released" << endl;
			Keys[0x4] = 0;
			break;

		case sf::Keyboard::W:
			cout << "W key released" << endl;
			Keys[0x5] = 0;
			break;

		case sf::Keyboard::E:
			cout << "E key released" << endl;
			Keys[0x6] = 0;
			break;

		case sf::Keyboard::R:
			cout << "R key released" << endl;
			Keys[0x7] = 0;
			break;

		case sf::Keyboard::A:
			cout << "A key released" << endl;
			Keys[0x8] = 0;
			break;

		case sf::Keyboard::S:
			cout << "S key released" << endl;
			Keys[0x9] = 0;
			break;

		case sf::Keyboard::D:
			cout << "D key released" << endl;
			Keys[0xA] = 0;
			break;

		case sf::Keyboard::F:
			cout << "F key released" << endl;
			Keys[0xB] = 0;
			break;

		case sf::Keyboard::Z:
			cout << "Z key released" << endl;
			Keys[0xC] = 0;
			break;

		case sf::Keyboard::X:
			cout << "X key released" << endl;
			Keys[0xD] = 0;
			break;

		case sf::Keyboard::C:
			cout << "C key released" << endl;
			Keys[0xE] = 0;
			break;

		case sf::Keyboard::V:
			cout << "V key released" << endl;
			Keys[0xF] = 0;
			break;

		default:
			cout << "unknown key released" << endl;
			break;
		}
	}	

	void resetState()
	{
		registerI = 0;
		PC = 0x200;
		memset(V, 0, sizeof(V));

		memcpy(&memory[0], &fonts, sizeof(fonts));

		string ibmLogo = "IBGM Logo.ch8";
		string testOpcode = "test_opcode.ch8";
		string pong = "Pong.ch8";
		string tetris = "Tetris.ch8";
		string invader = "invaders.c8";

		ifstream in;
		in.open(invader, ios::binary | ios::in | ios::ate);

		if (in.is_open())
		{
			cout << "Roam Loaded" << endl;
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
	chip.initAudio();

	sf::Clock clock;
	sf::Time tpf = sf::seconds(1.f / 1000);
	sf::Time accumulator = sf::Time::Zero;
	sf::Time timerTpf = sf::seconds(1.f / 60.f);
	sf::Time timerAccumulator = sf::Time::Zero;

	while (chip.screen.window.isOpen())
	{
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
			if (event.type == sf::Event::KeyReleased)
			{
				chip.releaseInput(event.key.code);
			}
		}

		if (accumulator >= tpf)
		{
			chip.fetch();
			accumulator = sf::Time::Zero;
		}

		// Update timers
		if (timerAccumulator >= timerTpf)
		{
			if (chip.delayTimer > 0)
			{
				chip.delayTimer--;
			}
			if (chip.soundTimer > 0)
			{
				
				if (chip.audio.sound.getStatus() != sf::Sound::Playing)
				{
					chip.audio.sound.play();
				}
					chip.soundTimer--;
			}
			else
			{
				if (chip.audio.sound.getStatus() == sf::Sound::Playing)
				{
					chip.audio.sound.stop();
				}
			}
			timerAccumulator = sf::Time::Zero;
		}

		if (chip.draw)
		{
			chip.renderPixels();
		}
		accumulator += clock.getElapsedTime();
		timerAccumulator += clock.restart();
	}
	return 0;
}