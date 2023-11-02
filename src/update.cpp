
#include "game.hpp"

void UpdateCar(Context &ctx, const CarInputs &inputs, CarData &car) {
  const auto accum = [](float t, float m) -> float { return t / m; };
  const float coef_cs = 0.01f;
  const float coef_ds = 0.98f;
  const float dt = 0.5f;
  // const float vl = 2.0f * (1.0f - expf(-Vector2Length(car.speed)));
  const float vl = 2.0f * (1.0f - expf(-0.2f * Vector2Length(car.speed)));
  const float cs = coef_cs * vl;
  const float dx = cos(car.dir);
  const float dy = sin(car.dir);
  const float acc = 0.4f * accum(car.thrust, 100.0f);
  const float dec = 0.2f * (1 - inputs.cthrust) * accum(inputs.brake, 1.0f);
  car.thrust = std::max(0.0f, car.thrust - 5.0f * inputs.brake);
  car.thrust = car.thrust + inputs.cthrust * expf(-0.001f * car.thrust);
  car.thrust = std::max(0.0f, car.thrust - (1.0f - inputs.cthrust));
  car.slide = std::min(1.0f, (0.4f * inputs.brake + 0.1f * inputs.cthrust) *
                                 (1.0f + abs(inputs.cwheel)));
  car.dir += cs * (1 + 0.3f * inputs.brake) * inputs.cwheel;
  car.speed = coef_ds * car.speed + (acc - dec) * Vector2{dx, dy};
  const auto npos = car.pos + dt * GameScale * car.speed;
  car.delta = npos - car.pos;
  car.pos = npos;
}

bool Update_Main(Context &ctx) {
  if (IsGamepadAvailable(0)) {
    if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT)) {
      ctx.state = State::PlayerSelect;
    }
  }
  if (IsKeyDown(KEY_ESCAPE))
    return false;
  return !WindowShouldClose();
}

bool Update_PlayerSelect(Context &ctx) {
  for (int i = 0; i < 8; ++i) {
    if (IsGamepadAvailable(i)) {
      if (IsGamepadButtonPressed(i, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) {
        int &pidx = ctx.ctrlToPlayer[i];
        if (pidx == 0) {
          auto itf = std::find_if(ctx.players.begin(), ctx.players.end(),
                                  [](Player &p) { return !p.enabled; });
          if (itf != ctx.players.end()) {
            size_t idx = 0;
            ctx.players[idx].enabled = true;
            pidx = idx + 1;
          }
        }
      } else if (IsGamepadButtonPressed(i, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT)) {
        int &pidx = ctx.ctrlToPlayer[i];
        if (pidx != 0) {
          size_t idx = pidx - 1;
          ctx.players[idx].enabled = false;
          ctx.ctrlToPlayer.erase(i);
        }
      }
    }
  }
  if (IsGamepadAvailable(0)) {
    if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT)) {
      InitCars(ctx);
      if (!ctx.cars.empty())
        ctx.state = State::Race;
    }
  }
  return !WindowShouldClose();
}

void UpdateCheckPoint(Context &ctx, Car &car, Vector2 lastPos, Vector2 newPos) {
  const auto &cps = ctx.track.checkpoints;
  const auto countCP = cps.size();
  auto &player = ctx.players[*car.playerIndex];
  const size_t startCP = (player.lastCP + 1) % countCP;
  for (size_t i = 0; i < countCP; ++i) {
    const auto &cp = cps[(startCP + i) % countCP];
    const auto nsz = Vector2LengthSqr(cp.delta);
    const auto dir = Vector2Normalize({-cp.delta.y, cp.delta.x});
    const float dp0 = Vector2DotProduct(lastPos - cp.pos, dir);
    const float dp1 = Vector2DotProduct(newPos - cp.pos, dir);
    if (dp0 * dp1 < 0) {
      const float r0 = Vector2DotProduct(lastPos - cp.pos, cp.delta) / nsz;
      const float r1 = Vector2DotProduct(lastPos - cp.pos, cp.delta) / nsz;
      if (abs(r0) < 1 && abs(r1) < 1) {
        TraceLog(LOG_INFO, "pass! %d", startCP + i);
        if (i == 0) {
          player.lastCP = startCP;
          if (startCP == 0) {
            if (player.startFrame) {
              const int frames = ctx.frame - *player.startFrame;
              ctx.bestChrono =
                  std::min(frames, ctx.bestChrono.value_or(frames));
              player.bestChrono =
                  std::min(frames, player.bestChrono.value_or(frames));
            }
            player.startFrame = ctx.frame;
          }
        } else {
          // reset pos
        }
        break;
      }
    }
  }
}

void SpawnParticles(Context &ctx, Car &car) {
  const auto r = []() {
    const int v = 18;
    return Vector2{
        float(GetRandomValue(-v, v)),
        float(GetRandomValue(-v, v)),
    };
  };
  const Vector2 d{
      cosf(car.data.dir),
      sinf(car.data.dir),
  };
  const Vector2 n{d.y, -d.x};
  const float l = GameScale * 25.0f;
  const float w = GameScale * 15.0f;
  const float str = car.data.slide;
  ctx.particles.push_back({car.data.pos + l * d + w * n,
                           r() + (45.0f / GameScale) * car.data.delta,
                           ctx.gtime, str});
  ctx.particles.push_back({car.data.pos - l * d + w * n,
                           r() + (45.0f / GameScale) * car.data.delta,
                           ctx.gtime, str});
  ctx.particles.push_back({car.data.pos + l * d - w * n,
                           r() + (45.0f / GameScale) * car.data.delta,
                           ctx.gtime, str});
  ctx.particles.push_back({car.data.pos - l * d - w * n,
                           r() + (45.0f / GameScale) * car.data.delta,
                           ctx.gtime, str});
}

bool Update_Race(Context &ctx) {
  if (IsGamepadAvailable(0)) {
    if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT)) {
      ctx.pause = !ctx.pause;
    }
  }

  if (!ctx.pause) {
    ctx.gtime += 1 / 60.0;

    for (Particle &p : ctx.particles) {
      const float r = float((ctx.gtime - p.t) / PLifeTime);
      p.pos = p.pos + (GameScale * (1 - r * r) / 60.0f) * p.speed;
    }

    ctx.particles.erase(std::remove_if(ctx.particles.begin(),
                                       ctx.particles.end(),
                                       [&](const Particle &p) -> bool {
                                         return p.t + PLifeTime < ctx.gtime;
                                       }),
                        ctx.particles.end());

    for (Car &car : ctx.cars) {
      if (!car.playerIndex)
        continue;
      const int gp = ctx.players[*car.playerIndex].gamepad;
      if (IsGamepadAvailable(gp)) {
        const float x = GetGamepadAxisMovement(gp, GAMEPAD_AXIS_LEFT_X);
        const float l = GetGamepadAxisMovement(gp, GAMEPAD_AXIS_LEFT_TRIGGER);
        const float r = GetGamepadAxisMovement(gp, GAMEPAD_AXIS_RIGHT_TRIGGER);
        const bool hb = IsGamepadButtonDown(gp, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
        const bool b = l > 0.0f;
        const bool s = r > 0.0f;
        CarInputs &inputs = car.inputs;
        inputs.cwheel = x;
        inputs.handbrake = hb ? 1.0f : 0.0f;
        inputs.cthrust = s ? 1.0f : 0.0f;
        inputs.brake = b ? 1.0f : 0.0f;
      }
    }
    for (Car &car : ctx.cars) {
      const auto lastPos = car.data.pos;
      UpdateCar(ctx, car.inputs, car.data);
      const auto newPos = car.data.pos;
      if (car.playerIndex)
        UpdateCheckPoint(ctx, car, lastPos, newPos);
      const bool slide = car.data.slide > 0.0f;
      if (slide)
        SpawnParticles(ctx, car);
    }
  }

  return !WindowShouldClose();
}

bool Update(Context &ctx) {
  using UpdateFn = bool(Context &);
  UpdateFn *const u[int(State::Count)] = {
      Update_Main,
      Update_PlayerSelect,
      Update_Race,
  };
  return u[int(ctx.state)](ctx);
}
