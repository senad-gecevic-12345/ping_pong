#include <cassert>
#include <array>
#include <queue>
#include <chrono>
#include <fstream>
#include <sstream>
#include <iostream>
#include <unordered_map>


#include <glad/glad.h> 
#include <GLFW/glfw3.h>


namespace{
    constexpr float g_coordinates_width { 800 };
    constexpr float g_coordinates_height{ 600 };
	float g_width{g_coordinates_width};
	float g_height{g_coordinates_height};

    constexpr float g_paddle_width{ 30 };
    constexpr float g_paddle_height{ 150 };
    constexpr float g_ball_diameter{ 25 };
    constexpr float g_ball_speed{ 21.5 };
    constexpr float g_paddle_speed{ 7 };

    const std::string g_shader_folder{"C:\\Users\\meme_\\Desktop\\ping_pong\\"};
}

namespace Shapes {

    std::array<float, 18> g_arr_rectangle{
         1.0f,  1.0f, 0.0f,  
         1.0f, -1.0f, 0.0f,  
        -1.0f, -1.0f, 0.0f,  
         1.0f,  1.0f, 0.0f,  
        -1.0f, -1.0f, 0.0f,  
        -1.0f,  1.0f, 0.0f    
    };

    std::array<float, 18> create_rectangle(int x, int y) {
        std::array<float, 18> arr = {};
        std::pair<float, float> aa = 
            std::make_pair( float(x) / g_coordinates_width, float(y) / g_coordinates_height);

        for (int i = 0; i < 18; ++i){
            arr[i] = g_arr_rectangle[i];
            if (i % 3 == 0) arr[i]      *= x;
            else if (i % 3 == 1) arr[i] *= y;
        }
        return arr;
    };


    const auto g_paddle_rectangle =			create_rectangle(g_paddle_width/2.f, g_paddle_height/2.f);
    const auto g_ball_rectangle =			create_rectangle(g_ball_diameter/2.f, g_ball_diameter/2.f);
    const auto g_overlay_rectangle =		create_rectangle(g_coordinates_width/2.f, g_coordinates_height/2.f);

};

struct stringbuilder
{
    std::stringstream ss;
    template<typename T>
    stringbuilder& operator << (const T& data)
    {
        ss << data;
        return *this;
    }
    operator std::string() { return ss.str(); }
};

struct EStart {
    enum {
        START = 0,
    };
};
struct EBall {
    enum {
        START = EStart::START,
        B = START,
        COUNT
    };
};
struct EPaddle{
    enum{
        START = EBall::COUNT,
        L = START,
        R,
        COUNT
    };
};
struct EScreenEffects{
    enum{
        START = EPaddle::COUNT,
        EFFECT = START,
        COUNT
    };
};
struct ETotal{
    enum{
        EntitiesCount = EScreenEffects::COUNT
    };
};

struct Rectangle{
    float width{0};
    float height{0};
};

struct Vec2{
    float x{0};
    float y{0};
};

struct ClampVal{
    float val;
    bool is_clamping;
};

struct Render {
    GLuint VAO, VBO;
    int vertex_count;
};

struct Flag {
	int flag{0};
};

struct Flags {
    enum {
        Collided = 1 << 0,
    };
};

struct Registry{
	std::unordered_map<int, bool> key_is_pressed;
    std::queue<Vec2> ball_positions;
    std::array<Flag,   ETotal::EntitiesCount> flags;
    std::array<Vec2,   ETotal::EntitiesCount> velocity;
    std::array<Vec2,   ETotal::EntitiesCount> position;
    std::array<Render,	  ETotal::EntitiesCount> render;
    std::array<Rectangle, ETotal::EntitiesCount > bounds;
}r;

Vec2 div(Vec2 a, float b) { return Vec2{ a.x / b, a.y / b }; }
Vec2 mul(Vec2 a, float b) { return Vec2{ a.x * b, a.y * b }; }
Vec2 plus(Vec2 a, Vec2 b)  { return Vec2{ a.x + b.x, a.y + b.y }; }
Vec2 minus(Vec2 a, Vec2 b) { return Vec2{ a.x - b.x, a.y - b.y }; }
float y_growth(Vec2 v) { return v.y / v.x; }
float x_growth(Vec2 v) { return v.x / v.y; }
float len(Vec2 a){ return sqrt((a.x*a.x) + (a.y*a.y)); }
float dot(Vec2 a, Vec2 b){ return (a.x*b.x) + (a.y*b.y); }
Vec2 norm(Vec2 a){
    auto l = len(a);
    return Vec2{ a.x / l, a.y / l };
}

void update_position(){
    for(int i = 0; i < ETotal::EntitiesCount; ++i){
        r.position[i] = plus(r.position[i], r.velocity[i]);
    }
}

ClampVal clamp(float val, float min, float max){
    if(val < min)
        return ClampVal{min, true};
    if(val > max)
        return ClampVal{max, true};
    return ClampVal{val, false};
}

bool float_is(float v, float comp, float epsilon = 0.001) {
    return (v + epsilon > comp && v - epsilon < comp);
}

bool vec2_is(Vec2 v, Vec2 comp) {
    return float_is(v.x, comp.x) && float_is(v.y, comp.y);
}

std::pair<ClampVal, ClampVal> ball_paddle_clamp(Vec2 paddle_pos, Rectangle paddle_size, Vec2 ball_pos, float circle_radius){
    Vec2 paddle_offset = { paddle_pos.x - (float(paddle_size.width) / 2), paddle_pos.y - (float(paddle_size.height) / 2) };
    auto x = clamp(ball_pos.x, paddle_offset.x, paddle_size.width  + paddle_offset.x);
    auto y = clamp(ball_pos.y, paddle_offset.y, paddle_size.height + paddle_offset.y);
    return std::make_pair(x, y);
}


void intersects_ball_paddle(int ball, int paddle) {
    Vec2 paddle_offset = { r.position[paddle].x - (float(r.bounds[paddle].width) / 2.f), r.position[paddle].y - (float(r.bounds[paddle].height) / 2.f) };
    auto x = clamp(r.position[ball].x, paddle_offset.x, r.bounds[paddle].width  + paddle_offset.x);
    auto y = clamp(r.position[ball].y, paddle_offset.y, r.bounds[paddle].height + paddle_offset.y);
    auto diff = minus(r.position[ball], Vec2{x.val, y.val});

    float ball_radius = r.bounds[ball].width / 2.f;
    constexpr float sin_60 = 0.866f;
    
    if (vec2_is(diff, { 0.0, 0.0 })) { 
        if (r.velocity[ball].x < 0) {
            x.val = paddle_offset.x + r.bounds[paddle].width;
            r.position[ball].x = x.val + (ball_radius / 2.f);
            // set to false because x must take priority
            y.is_clamping = false; 
        }
        else {
            x.val = paddle_offset.x;
            r.position[ball].x = x.val - (ball_radius / 2.f);
            y.is_clamping = false;
        }
        diff = minus(r.position[ball], Vec2{x.val, y.val});
    }
    if(auto move = ball_radius - len(diff); move > 0){
        float new_sign = r.velocity[ball].x < 0 ? 1.f : -1.f;
        r.position[ball] = plus(r.position[ball], mul(norm(diff), move));

        if (y.is_clamping) {
            // can intersect several times on near 0 y and then looks wrong so need this
			auto new_y =  r.position[ball].y > r.position[paddle].y ?  1.f : -1.f;
            r.velocity[ball].y = fabsf(r.velocity[ball].y) * new_y;
			r.flags[ball].flag |= Flags::Collided;
		   return;
        }

        float distance_to_mid = (r.position[ball].y - r.position[paddle].y) * (1.f / (r.bounds[paddle].height / 2.f));
        float s = distance_to_mid * sin_60;
        float cos = (sqrt(1.f - (s * s)));

        r.velocity[ball] = mul(Vec2{ (cos*new_sign), s }, g_ball_speed);
        r.flags[ball].flag |= Flags::Collided;

    }
}

void intersects_screen_check_handler(int id, float width, float height, bool flip = false){
    width /= 2.f;
    height /= 2.f;
    if(fabsf(r.position[id].y) + height > g_coordinates_height/2.f){
        if (r.position[id].y > 0)
            r.position[id].y = g_coordinates_height/2.f - height;
        else
            r.position[id].y = -g_coordinates_height/2.f + height;
        if(flip)
            r.velocity[id].y *= -1;
        r.flags[id].flag |= Flags::Collided;
    }
    if(fabsf(r.position[id].x) + width > g_coordinates_width/2.f){
        if (r.position[id].x > 0)
            r.position[id].x = g_coordinates_width/2.f - width;
        else
            r.position[id].x = -g_coordinates_width/2.f + width;
        if(flip)
            r.velocity[id].x *= -1;
        r.flags[id].flag |= Flags::Collided;
    }
}

std::string file_reader(const std::string& file_loc_name) {
    std::string out;
    std::ifstream file(file_loc_name);
    if(file.is_open()){
        std::string temp;
        while(getline(file, temp)){
            out.append(temp);
            out.append("\n");
        }
    }
    else {
        throw "Error Reading file";
    }
    file.close();
    return out;
}


struct ShaderProgram {
    int shader_program;
    ShaderProgram(const char* vert_char, const char* frag_char) {
        unsigned int fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
        unsigned int vertex_shader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(fragment_shader, 1, &frag_char, NULL);
        glShaderSource(vertex_shader, 1, &vert_char, NULL);
        glCompileShader(fragment_shader);
        int ok;
        char infoLog[512];
        glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &ok);
        if (!ok){
            glGetShaderInfoLog(fragment_shader, 512, NULL, infoLog);
            std::cerr << "fragment shader failed compilation\n" << infoLog << std::endl;
			throw "Shader Error";
        }
        glCompileShader(vertex_shader);
        glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &ok);
        if (!ok){
            glGetShaderInfoLog(vertex_shader, 512, NULL, infoLog);
            std::cerr << "vertex shader failed compilation\n" << infoLog << std::endl;
			throw "Shader Error";
        }
        shader_program = glCreateProgram();
        glAttachShader(shader_program, vertex_shader);
        glAttachShader(shader_program, fragment_shader);
        glLinkProgram(shader_program);
        glGetProgramiv(shader_program, GL_LINK_STATUS, &ok);
        if (!ok) {
            glGetProgramInfoLog(shader_program, 512, NULL, infoLog);
            std::cerr << "shader program linking failed\n" << infoLog << std::endl;
			throw "Shader Error";
        }
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
    }
    void use() {
        glUseProgram(shader_program);
    }
};

class ShaderProgramBall : public ShaderProgram{
    unsigned int resolution_loc;
    unsigned int radius_loc;
    unsigned int offset_loc;
public:
    ShaderProgramBall(const char* vert_char, const char* frag_char):ShaderProgram(vert_char, frag_char) {
        resolution_loc =    glGetUniformLocation(shader_program, "mResolution");
        radius_loc =		glGetUniformLocation(shader_program, "mRadius");
        offset_loc =		glGetUniformLocation(shader_program, "mOffset");
    }
    void uniform(float radius, Vec2 offset) {
        glUniform2f(offset_loc, offset.x, offset.y);
        glUniform1f(radius_loc, radius);
    }
    void use() {
        ShaderProgram::use();
        glUniform2f(resolution_loc, g_width, g_height);
    }
};

class ShaderProgramPaddle : public ShaderProgram{
    unsigned int resolution_loc;
    unsigned int offset_loc;
public:
    ShaderProgramPaddle(const char* vert_char, const char* frag_char):ShaderProgram(vert_char, frag_char) {
        resolution_loc =    glGetUniformLocation(shader_program, "mResolution");
        offset_loc =		glGetUniformLocation(shader_program, "mOffset");
    }
    void uniform(Vec2 offset) {
        glUniform2f(offset_loc, offset.x, offset.y);
    }
    void use() {
        ShaderProgram::use();
        glUniform2f(resolution_loc, g_width, g_height);
    }
};

class ShaderProgramEffects : public ShaderProgram{
    unsigned int resolution_loc;
    unsigned int time_loc;
    static constexpr int array_sizes{10};
    std::array<unsigned int, array_sizes> array_loc;
    std::array<Vec2, array_sizes> array_values;
public:
    ShaderProgramEffects(const char* vert_char, const char* frag_char):ShaderProgram(vert_char, frag_char) {
        resolution_loc =    glGetUniformLocation(shader_program, "mResolution");
        time_loc =          glGetUniformLocation(shader_program, "mTime");
        for(int i = 0; i < array_sizes; ++i){
            std::string uniform = stringbuilder() << "mArr[" << i << "]";
            array_loc[i] = glGetUniformLocation(shader_program, uniform.c_str());
        }
    }
    void uniform(std::queue<Vec2> pos) {
        if (pos.size() != 10) return;
        for (int i = 0; i < 10; ++i) {
            array_values[i] = pos.front();
            pos.pop();
        }
        for (int i = 0; i < array_loc.size(); ++i) {
            glUniform2f(array_loc[i], array_values[i].x, array_values[i].y);
        }
    }
    void use(float time) {
        ShaderProgram::use();
        glUniform1f(time_loc, time);
        glUniform2f(resolution_loc, g_width, g_height);
    }
};

void create_render_rectangle(Render& render, const std::array<float, 18>& rectangle) {
    render.vertex_count = 6;
    glGenVertexArrays(1, &render.VAO);
    glGenBuffers(1, &render.VBO);
    glBindVertexArray(render.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, render.VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(rectangle), &rectangle[0], GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	int speed = g_paddle_speed;
	if(action == GLFW_PRESS || action == GLFW_RELEASE)
		r.key_is_pressed[key] = action;
	float velocity = 0;

	if(r.key_is_pressed[GLFW_KEY_DOWN]){
		velocity -= speed;
	}
	if (r.key_is_pressed[GLFW_KEY_UP]) {
		velocity += speed;
	}
	r.velocity[EPaddle::L].y = velocity;

}

void initialize_renders() {
    for (int i = EBall::START; i < EBall::COUNT; ++i) {
        create_render_rectangle(r.render[i], Shapes::g_ball_rectangle);
    }
    for (int i = EPaddle::START; i < EPaddle::COUNT; ++i) {
        create_render_rectangle(r.render[i], Shapes::g_paddle_rectangle);
    }
    for (int i = EScreenEffects::START; i < EScreenEffects::COUNT; ++i) {
        create_render_rectangle(r.render[i], Shapes::g_overlay_rectangle);
    }
}
void initialize_bounds() {
    for(int i = EBall::START; i < EBall::COUNT; ++i)
        r.bounds[i]   = { g_ball_diameter, g_ball_diameter };
    for(int i = EPaddle::START; i < EPaddle::COUNT; ++i)
        r.bounds[i] =   { g_paddle_width, g_paddle_height };
}

void initialize_values() {
    r.velocity[EBall::B].x = 12.5 + 5;
    r.velocity[EBall::B].y = 7.5 + 5;
    r.velocity[EBall::B] = mul(norm(r.velocity[EBall::B]), g_ball_speed);

    r.position[EPaddle::L].x = -280;
    r.position[EPaddle::R].x = 280;

    for (int i = 0; i < 10; ++i)
        r.ball_positions.push(r.position[EBall::B]);
}

void initialize() {
    initialize_renders();
    initialize_bounds();
    initialize_values();
}


void update_right_paddle() {
    auto bp = r.position[EBall::B];
    auto rp = r.position[EPaddle::R];
    auto diff = minus(rp, bp);

    if (vec2_is(diff, { 0, 0 })) return;
	if ((bp.x > rp.x && fabsf(bp.y - rp.y) < r.bounds[EPaddle::R].height / 2)) {
		if(rp.y > 0)
			r.velocity[EPaddle::R].y = -g_paddle_speed;
		else
			r.velocity[EPaddle::R].y = g_paddle_speed;
		return;
	}
    if (r.velocity[EBall::B].x < 0) {
        r.velocity[EPaddle::R] = { 0, 0 };
        return;
    }
	if (float_is(r.velocity[EBall::B].x, 0, 0.1)) {
		if(r.velocity[EBall::B] > 0)
			r.velocity[EPaddle::R].y = g_paddle_speed;
		else
			r.velocity[EPaddle::R].y = -g_paddle_speed;
		return;
	}

    auto yg = y_growth(r.velocity[EBall::B]);
    auto bs = len({ diff.x, yg * diff.x }) / g_ball_speed;
    auto y = (yg * diff.x) + bp.y;
    auto ps = fabsf(y - rp.y) / g_paddle_speed;
    auto d = clamp(ps / bs, 0, 1);

    if (y > rp.y) 
        r.velocity[EPaddle::R].y = (g_paddle_speed * d.val);
    else 
        r.velocity[EPaddle::R].y = -(g_paddle_speed * d.val);
    
}

// basis res hardcoded to shaders for accurate scaling
void window_size_callback(GLFWwindow* window, int width, int height)
{
    g_width = width;
    g_height = height;
    glViewport(0, 0, width, height);
}

int main(){

    if (!glfwInit())return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(g_coordinates_width, g_coordinates_height, "window", NULL, NULL);

    if (window == NULL){
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, key_callback);
    glfwSetWindowSizeCallback(window, window_size_callback);
    glfwSetInputMode(window, GLFW_STICKY_KEYS, GLFW_TRUE);
	glfwSetWindowAspectRatio(window, 4, 3);

    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);

    const auto ball_vertex_shader =     file_reader(g_shader_folder + "ball.vs");
    const auto ball_fragment_shader =   file_reader(g_shader_folder + "ball.fs");
    const auto paddle_vertex_shader =   file_reader(g_shader_folder + "paddle.vs");
    const auto paddle_fragment_shader = file_reader(g_shader_folder + "paddle.fs");
    const auto effect_vertex_shader =     file_reader(g_shader_folder + "effect.vs");
    const auto effect_fragment_shader =   file_reader(g_shader_folder + "effect.fs");

    ShaderProgramBall ball_shader(ball_vertex_shader.c_str(), ball_fragment_shader.c_str());
    ShaderProgramPaddle paddle_shader(paddle_vertex_shader.c_str(), paddle_fragment_shader.c_str());
    ShaderProgramEffects effect_shader(effect_vertex_shader.c_str(), effect_fragment_shader.c_str());


    initialize();

    auto last_tick{ std::chrono::high_resolution_clock::now() };
    double time_in_seconds{ 0.0 };
    while (!glfwWindowShouldClose(window))
    {
        auto current_tick{ std::chrono::high_resolution_clock::now() };
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(current_tick - last_tick);
        if (ms <= std::chrono::milliseconds(1000/60))continue;
        time_in_seconds += double(ms.count()) / 1000.f;
        last_tick = current_tick;
        if ((r.flags[EBall::B].flag & Flags::Collided) == Flags::Collided)
            update_right_paddle();
        for (auto& x : r.flags)
            x.flag = 0;

        update_position();
        for(int i = EPaddle::START; i < EPaddle::COUNT; ++i)
            intersects_ball_paddle(EBall::B, i);
        for(int i = EBall::START; i < EBall::COUNT; ++i)
            intersects_screen_check_handler(i, r.bounds[i].width, r.bounds[i].height, true);
        for(int i = EPaddle::START; i < EPaddle::COUNT; ++i)
            intersects_screen_check_handler(i, r.bounds[i].width, r.bounds[i].height);


        glClearColor(0.2f, 0.2f, 0.4f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        effect_shader.use(time_in_seconds);
        {
            auto& render = r.render[EScreenEffects::EFFECT];
            effect_shader.uniform(r.ball_positions);
            glBindVertexArray(render.VAO); 
            glDrawArrays(GL_TRIANGLES, 0, render.vertex_count);
        }

        ball_shader.use();
        { 
            Vec2 offset = r.position[EBall::B];
            r.ball_positions.pop();
            r.ball_positions.push(offset);
                
            auto& render = r.render[EBall::B];
            ball_shader.uniform(r.bounds[EBall::B].width/2.f, offset);
            glBindVertexArray(render.VAO); 
            glDrawArrays(GL_TRIANGLES, 0, render.vertex_count);
        }

        paddle_shader.use();
        for (int i = EPaddle::START; i < EPaddle::COUNT; ++i) {
            Vec2 offset = r.position[i];
            auto& render = r.render[i];
            paddle_shader.uniform(offset);
            glBindVertexArray(render.VAO); 
            glDrawArrays(GL_TRIANGLES, 0, render.vertex_count);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
}

