#include "SDL2/SDL.h"
#include "SDL2/SDL_ttf.h"
#include "SDL2/SDL_video.h"
#include "SDL2/SDL2_gfxPrimitives.h"
#include "SDL2/SDL_mixer.h"
#include "chrono"
#include "string"

const int WINDOW_HEIGHT = 720;
const int WINDOW_WIDTH  = 1280;

enum Buttons{
	PaddleOneUp = 0,
	PaddleOneDown,
	PaddleTwoUp,
	PaddleTwoDown,
};

enum class collisionType{
	None,
	Top,
	Middle,
	Bottom,
	Left,
	Right,
};

const float PADDLE_SPEED = 1.0f;
const float BALL_SPEED = 0.65f;

struct Contact{
	collisionType type;
	float penetration;
};

class vector2d {
	public:
		vector2d(float inpx = 0, float inpy = 0)
			: x(inpx), y(inpy)
		{}
		vector2d operator+(vector2d const& rhs){
			return vector2d(x + rhs.x, y + rhs.y);
		}

		vector2d& operator+=(vector2d const& rhs){
			x += rhs.x;
			y += rhs.y;

			return *this;
		}
		
		vector2d operator* (float rhs){
			return vector2d(x * rhs, y * rhs);
		}
		float x,y;
};

const int BALL_WIDTH  = 15;
const int BALL_HEIGHT = 15;

class Ball {
	public:
		Ball(vector2d position, vector2d velocity)
			: position(position), velocity(velocity)
		{}

		void CollideWithPaddle(Contact const& contact){
			position.x += contact.penetration;
			velocity.x = -velocity.x;

			if(contact.type == collisionType::Top){
				velocity.y = -.75f * BALL_SPEED;
			}
			else if(contact.type == collisionType::Bottom){
				velocity.y = 0.75f * BALL_SPEED;
			}
		}

		void CollideWithWall(Contact const& contact){
			if((contact.type == collisionType::Top)
				|| (contact.type == collisionType::Bottom)){
				position.y += contact.penetration;
				velocity.y = -velocity.y;
			}
			else if(contact.type == collisionType::Left){
				position.x = WINDOW_WIDTH / 2.0f;
				position.y = WINDOW_HEIGHT / 2.0f;
				velocity.x = BALL_SPEED;
				velocity.y = 0.75f * BALL_SPEED;
			}
			else if(contact.type == collisionType::Right){
				position.x = WINDOW_WIDTH / 2.0f;
				position.y = WINDOW_HEIGHT / 2.0f;
				velocity.x = -BALL_SPEED;
				velocity.y = 0.75 * BALL_SPEED;
			}
		}

		void update(float dt){
			position += velocity * dt;
		}

		void draw(SDL_Renderer* renderer, uint32_t color){
			filledCircleColor(renderer, position.x, position.y, BALL_WIDTH, color);
		}
		vector2d velocity;
		vector2d position;
};

const int PADDLE_HEIGHT = 100;
const int PADDLE_WIDTH  = 10;

class Paddle{
	public:
		Paddle(vector2d position, vector2d velocity)
			: position(position), velocity(velocity)
		{}

		void update(float dt){
			position += velocity * dt;
			if(position.y < 0){
				//restrict to top of screen;
				position.y = 0;
			}
			else if(position.y > (WINDOW_HEIGHT - PADDLE_HEIGHT)){
				//restrict to bottom of screen
				position.y = WINDOW_HEIGHT - PADDLE_HEIGHT;
			}
		}

		void draw(SDL_Renderer* renderer, uint32_t color){
			boxColor(renderer, position.x, position.y, position.x + PADDLE_WIDTH, position.y + PADDLE_HEIGHT, color);
		}
		vector2d position;
		vector2d velocity;
};

class PlayerScore{
	public:
		PlayerScore(vector2d position, SDL_Renderer* renderer, TTF_Font* font)
			:position(position), renderer(renderer), font(font)
		{
			surface = TTF_RenderText_Solid(font, "0", {0xFF, 0xFF, 0xFF, 0xFF});
			texture = SDL_CreateTextureFromSurface(renderer, surface);

			int width, height;
			SDL_QueryTexture(texture, nullptr, nullptr, &width, &height);

			rect.x = static_cast<int>(position.x);
			rect.y = static_cast<int>(position.y);
			rect.w = width;
			rect.h = height;
		}
		
		~PlayerScore(){
			SDL_FreeSurface(surface);
			SDL_DestroyTexture(texture);
		}

		void draw(){
			SDL_RenderCopy(renderer, texture, nullptr, &rect);
		}

		void SetScore(int score){
			SDL_FreeSurface(surface);
			SDL_DestroyTexture(texture);

			surface = TTF_RenderText_Solid(font, std::to_string(score).c_str(), {0xFF, 0xFF, 0xFF, 0xFF});
			texture = SDL_CreateTextureFromSurface(renderer, surface);

			int width, height;
			SDL_QueryTexture(texture, nullptr, nullptr, &width, &height);
			rect.w = width;
			rect.h = height;
		}
		
		vector2d position;
		SDL_Renderer* renderer;
		TTF_Font* font;
		SDL_Surface* surface{};
		SDL_Texture* texture{};
		SDL_Rect rect{};
};

Contact checkPaddleCollision(Ball const& ball, Paddle const& paddle){
	float ballLeft = ball.position.x;
	float ballRight = ball.position.x + BALL_WIDTH;
	float ballTop = ball.position.y;
	float ballBottom = ball.position.y + BALL_HEIGHT;

	float paddleLeft = paddle.position.x;
	float paddleRight = paddle.position.x + PADDLE_WIDTH;
	float paddleTop = paddle.position.y;
	float paddleBottom = paddle.position.y + PADDLE_HEIGHT;

	Contact contact{};

	if (ballLeft >= paddleRight){
		return contact;
	}

	if (ballRight <= paddleLeft){
		return contact;
	}

	if (ballTop >= paddleBottom){
		return contact;
	}

	if (ballBottom <= paddleTop){
		return contact;
	}
	
	float paddleRangeUpper = paddleBottom - (2.0f * PADDLE_HEIGHT / 3.0f);
	float paddleRangeMiddle = paddleBottom - (PADDLE_HEIGHT / 3.0f);

	if(ball.velocity.x<0){
		//left paddle
		contact.penetration = paddleRight - ballLeft;
	}
	else if(ball.velocity.x>0){
		//right paddle
		contact.penetration = paddleLeft - ballRight;
	}
	if((ballBottom > paddleTop)
		&& (ballBottom < paddleRangeUpper)){
		contact.type = collisionType::Top;
	}
	else if((ballBottom > paddleRangeUpper)
		&& (ballBottom < paddleRangeMiddle)){
		contact.type = collisionType::Middle;
	}
	else{
		contact.type = collisionType::Bottom;
	}

	return contact;
}

Contact checkWallCollision (Ball const& ball){
	float ballLeft = ball.position.x;
	float ballRight = ball.position.x + BALL_WIDTH;
	float ballTop = ball.position.y;
	float ballBottom = ball.position.y + BALL_HEIGHT;

	Contact contact{};

	if (ballLeft < 0.0f){
		contact.type = collisionType::Left;
	}
	else if(ballRight > WINDOW_WIDTH){
		contact.type = collisionType::Right;
	}
	else if(ballTop < 0.0f){
		contact.type = collisionType::Top;
		contact.penetration = -ballTop;
	}
	else if(ballBottom > WINDOW_HEIGHT){
		contact.type = collisionType::Bottom;
		contact.penetration = WINDOW_HEIGHT - ballBottom;
	}
	
	return contact;
}

int main(){
	//initialize SDL
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
	TTF_Init();
	Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048);

	SDL_Window* window = SDL_CreateWindow("Pong" , 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 0);
	
	//initialize font
	TTF_Font* scoreFont = TTF_OpenFont("DejaVuSansMono.ttf", 40);

	//initialize sound effects
	Mix_Chunk* wallHitSound = Mix_LoadWAV("WallHit.wav");
	Mix_Chunk* paddleHitSound = Mix_LoadWAV("PaddleHit.wav");
	
	//create player score text fields
	PlayerScore playerOneScoreText(vector2d(WINDOW_WIDTH / 4.0f, 20), renderer, scoreFont);
	PlayerScore playerTwoScoreText(vector2d(3.0f * WINDOW_WIDTH / 4.0f, 20), renderer, scoreFont);

	//create the Ball
	Ball ball(
		vector2d((WINDOW_WIDTH / 2.0f) - (BALL_WIDTH / 2.0f),
		(WINDOW_HEIGHT / 2.0f) - (BALL_HEIGHT / 2.0f)),
		vector2d(BALL_SPEED, 0.0f));
	
	//create the paddles
	Paddle paddleOne(
			vector2d(50.0f, (WINDOW_HEIGHT / 2.0f) - (PADDLE_HEIGHT / 2.0f)),
			vector2d(0.0f, 0.0f));
	Paddle paddleTwo(
			vector2d(WINDOW_WIDTH - 50.0f, (WINDOW_HEIGHT / 2.0f) - (PADDLE_HEIGHT / 2.0f)),
			vector2d(0.0f,0.0f));

	//game logic
	{
		int playerOneScore = 0;
		int playerTwoScore = 0;

		bool running = true;
		bool buttons[4] = {};

		float dt = 0.0f;

		//continue the loop until user exits
		while(running){
			
			auto startTime = std::chrono::high_resolution_clock::now();

			SDL_Event event;
			while(SDL_PollEvent(&event)){
				if(event.type == SDL_QUIT){
					running = false;
				}
				else if (event.type == SDL_KEYDOWN){
					if(event.key.keysym.sym == SDLK_ESCAPE){
						running = false;
					}
					else if (event.key.keysym.sym == SDLK_w){
						buttons[Buttons::PaddleOneUp] = true;
					}
					else if (event.key.keysym.sym == SDLK_s){
						buttons[Buttons::PaddleOneDown] = true;
					}
					else if (event.key.keysym.sym == SDLK_UP){
						buttons[Buttons::PaddleTwoUp] = true;
					}
					else if (event.key.keysym.sym == SDLK_DOWN){
						buttons[Buttons::PaddleTwoDown] = true;
					}
				}
				else if(event.type == SDL_KEYUP){
					if (event.key.keysym.sym == SDLK_w){
						buttons[Buttons::PaddleOneUp] = false;
					}
					else if (event.key.keysym.sym == SDLK_s){
						buttons[Buttons::PaddleOneDown] = false;
					}
					else if (event.key.keysym.sym == SDLK_UP){
						buttons[Buttons::PaddleTwoUp] = false;
					}
					else if (event.key.keysym.sym == SDLK_DOWN){
						buttons[Buttons::PaddleTwoDown] = false;
					}
				}
			}

			//clear window to black

			SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xFF);
			SDL_RenderClear(renderer);

			//set draw color to white
			SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);

			//draw net
			for(int y=0; y < WINDOW_HEIGHT; y++){
				if(y%7){
					SDL_RenderDrawPoint(renderer,WINDOW_WIDTH/2,y);
				}
			}
			
			if (buttons[Buttons::PaddleOneUp]){
				paddleOne.velocity.y = -PADDLE_SPEED;
			}
			else if (buttons[Buttons::PaddleOneDown]){
				paddleOne.velocity.y = PADDLE_SPEED;
			}
			else{
				paddleOne.velocity.y = 0.0f;
			}

			if (buttons[Buttons::PaddleTwoUp]){
				paddleTwo.velocity.y = -PADDLE_SPEED;
			}
			else if (buttons[Buttons::PaddleTwoDown]){
				paddleTwo.velocity.y = PADDLE_SPEED;
			}
			else{
				paddleTwo.velocity.y = 0.0f;
			}
			// Update the paddle positions
			paddleOne.update(dt);
			paddleTwo.update(dt);

			//update ball position
			ball.update(dt);

			//check collisions
			Contact contact = checkPaddleCollision(ball, paddleOne);
			if(contact.type != collisionType::None){
				ball.CollideWithPaddle(contact);
				Mix_PlayChannel(-1, paddleHitSound, 0);
			}
			else {contact = checkPaddleCollision(ball, paddleTwo);
			if(contact.type != collisionType::None){
				ball.CollideWithPaddle(contact);
				Mix_PlayChannel(-1, paddleHitSound, 0);
			}
			else {contact = checkWallCollision(ball);
			if(contact.type != collisionType::None){
				ball.CollideWithWall(contact);

				if(contact.type == collisionType::Left){
					++playerTwoScore;
					playerTwoScoreText.SetScore(playerTwoScore);
				}
				else if(contact.type == collisionType::Right){
					++playerOneScore;
					playerOneScoreText.SetScore(playerOneScore);
				}
				else{
					Mix_PlayChannel(-1, wallHitSound, 0);
				}
			}}}

			//Draw the ball
			ball.draw(renderer, 0xFFFFFFFF);
			
			//draw the paddles
			paddleOne.draw(renderer, 0xFFFFFFFF);
			paddleTwo.draw(renderer, 0xFFFFFFFF);
			playerTwoScoreText.draw();
			playerOneScoreText.draw();

			//present the backbuffer
			SDL_RenderPresent(renderer);
				
			//calculate frame time
			auto stopTime = std::chrono::high_resolution_clock::now();
			dt = std::chrono::duration<float, std::chrono::milliseconds::period>(stopTime - startTime).count();
		}
	}

	//cleanup
	Mix_FreeChunk(wallHitSound);
	Mix_FreeChunk(paddleHitSound);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	TTF_CloseFont(scoreFont);
	TTF_Quit();
	SDL_Quit();

	return 0;
}
