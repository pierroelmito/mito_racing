#pragma once

// C
#include <cstdint>
#include <cstdio>

// C++
#include <algorithm>
#include <array>
#include <iterator>
#include <map>
#include <optional>
#include <vector>

// raylib
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

constexpr double PLifeTime = 1.5;
constexpr float GameScale = 0.1f;

enum class State {
  Main,
  PlayerSelect,
  Race,
  Count,
};

struct CarInputs {
  float cwheel{};
  float cthrust{};
  float brake{};
  float handbrake{};
};

struct CarData {
  Vector2 pos{};
  Vector2 delta{};
  Vector2 speed{};
  float thrust{};
  float dir{};
  float slide{};
};

struct Car {
  CarInputs inputs{};
  CarData data{};
  int model{};
  std::optional<int> playerIndex{};
};

struct Player {
  bool enabled{};
  int gamepad{};
  int lastCP{-1};
  std::optional<int> startFrame{};
  std::optional<int> bestChrono{};
};

struct Particle {
  Vector2 pos{};
  Vector2 speed{};
  double t{};
  float str{};
};

struct Checkpoint {
  Vector2 pos{};
  Vector2 delta{};
};

struct Track {
  std::vector<Vector2> track{};
  std::vector<Model> models{};
  std::vector<std::tuple<Vector3, float, Model *>> props{};
  std::vector<Checkpoint> checkpoints{};
  Rectangle aabb{};
};

struct Context {
  const int W = 1280;
  const int H = 720;
  State state{State::Main};
  Texture trackTex{};
  std::vector<Texture> voxelTex{};
  Texture noiseTex{};
  std::map<int, int> ctrlToPlayer{};
  std::vector<Car> cars{};
  std::array<Player, 4> players{};
  bool pause{};
  Track track{};
  std::vector<int> checkPointsChrono{};
  std::optional<int> bestChrono{};
  Shader shdGround{};
  Shader shdInstancing{};
  std::vector<Model> mdlCars{};
  Model mdlGround{};
  Model mdlParticle{};
  Model mdlTree{};
  std::vector<RenderTexture> rts{};
  std::vector<Particle> particles{};
  double gtime{};
  int frame{};
  bool showDebug{};
};

inline Vector2 operator+(Vector2 v0, Vector2 v1) { return Vector2Add(v0, v1); }
inline Vector3 operator+(Vector3 v0, Vector3 v1) { return Vector3Add(v0, v1); }
inline Vector2 operator-(Vector2 v0, Vector2 v1) {
  return Vector2Subtract(v0, v1);
}
inline Vector3 operator-(Vector3 v0, Vector3 v1) {
  return Vector3Subtract(v0, v1);
}
inline Vector2 operator*(float v0, Vector2 v1) { return Vector2Scale(v1, v0); }
inline Vector3 operator*(float v0, Vector3 v1) { return Vector3Scale(v1, v0); }

inline std::optional<Vector2> deadZone(Vector2 v, float ml) {
  const float cl = Vector2LengthSqr(v);
  if (cl < ml * ml)
    return {};
  return std::make_optional((1.0f / sqrtf(cl)) * v);
}

inline void SetState(Context &ctx, State s) { ctx.state = s; }

void InitCars(Context &ctx);
void Init(Context &ctx, int argc, char **argv);
bool Update(Context &ctx);
void Render(Context &ctx);
