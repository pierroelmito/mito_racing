
#include "game.hpp"

void UpdateRtSize(RenderTexture &rt, int w, int h) {
  if (rt.texture.width == w && rt.texture.height == h)
    return;
  TraceLog(LOG_INFO, "rebuild RT %dx%d", w, h);
  if (rt.texture.width != 0)
    UnloadRenderTexture(rt);
  rt = LoadRenderTexture(w, h);
}

template <typename... T>
int MyDrawText(int x, int y, Color col, int sz, const char *fmt, T &&...args) {
  char txt[512]{};
  snprintf(txt, sizeof(txt), fmt, args...);
  DrawText(txt, x, y, sz, col);
  return y + sz;
}

std::string StepToChrono(int steps) {
  const int csec = (steps * 10) / 6;
  const int a = csec % 100;
  const int b = (csec / 100) % 60;
  const int c = (csec / 100) / 60;
  char buffer[128]{};
  snprintf(buffer, sizeof(buffer), "%4d : %02d : %02d", c, b, a);
  return buffer;
}

struct View3D {

  static Camera3D GetCamera(Context &ctx, const Car &car, Vector2 dim) {
    Camera3D cam{};
    const float rtScale = 2.0f;
    Vector2 delta = 20.0f * car.data.speed;
    const float dl = Vector2Length(delta);
    const float maxdl = 560.0f / rtScale;
    if (dl > maxdl) {
      delta = (maxdl / dl) * delta;
    }
    const float scale = 0.1f + 0.2f * exp(-dl / maxdl);
    const Vector2 target = car.data.pos + GameScale * 2.5f * delta;
    const float width = (1.0f / scale) * 300.0f;
    const float cf0 = 300.0f;
    const float cf1 = 0.0f;
    const float o0 = 5.0f;
    const float o1 = 2.0f;
    const Vector3 dir = Vector3Normalize({0, o0, o1});
    cam = {
        Vector3{target.x, 0.0f, target.y} + cf0 * dir,
        Vector3{target.x, 0.0f, target.y} + cf1 * dir,
        Vector3Normalize({0.0f, o1, -o0}),
        GameScale * width,
        CAMERA_ORTHOGRAPHIC,
    };
    return cam;
  }

  static void RenderView(Context &ctx, const Car &car, Vector2 dim,
                         int &propCount) {
    const Camera3D cam = GetCamera(ctx, car, dim);
    ClearBackground(PINK);
    BeginMode3D(cam);
    {
      {
        const float ratio = dim.x / dim.y;
        const Vector3 eye = cam.target - cam.position;
        const Vector3 left = Vector3Normalize(Vector3CrossProduct(eye, cam.up));
        const Vector3 up = Vector3Normalize(Vector3CrossProduct(eye, left));
        for (const auto &prop : ctx.track.props) {
          const Vector3 &pos = std::get<0>(prop);
          const float scale = std::get<1>(prop);
          const Vector2 proj = {
              Vector3DotProduct(pos - cam.position, left),
              Vector3DotProduct(pos - cam.position, up),
          };
          if (abs(proj.x) > scale * 10.0f + ratio * cam.fovy / 2.0f ||
              abs(proj.y) > scale * 10.0f + cam.fovy / 2.0f)
            continue;
          propCount++;
          DrawModel(*std::get<2>(prop), pos, scale, BROWN);
        }
        for (const auto &checkppint : ctx.track.checkpoints) {
          const std::tuple<Vector2, Color> ps[2] = {
              {checkppint.pos + checkppint.delta, RED},
              {checkppint.pos - checkppint.delta, BLUE},
          };
          for (const auto &[p, c] : ps) {
            DrawCube({p.x, 0, p.y}, 4, 10, 4, c);
          }
        }
      }
      DrawModel(ctx.mdlGround, {0, -0.01f, 0}, 1, BROWN);
      for (const auto &mdl : ctx.track.models)
        DrawModel(mdl, {}, 1, WHITE);
      for (const Car &car : ctx.cars) {
        const float a = 180.0f + car.data.slide * GetRandomValue(-2, 2) +
                        car.data.dir * 180.0 / PI;
        DrawModelEx(ctx.mdlCars[car.model], {car.data.pos.x, 0, car.data.pos.y},
                    {0.0f, -1.0f, 0.0f}, a, {1.0f, 1.0f, 1.0f}, WHITE);
      }
      std::vector<Matrix> transforms;
      for (Particle &p : ctx.particles) {
        const float r = float((ctx.gtime - p.t) / PLifeTime);
        const float sz = GameScale * r * 32;
        const float a = p.str * (1 - r) * (155.0f / 255.0f);
        const Matrix m{
            sz, 0, 0, p.pos.x, 0, sz, 0, sz, 0, 0, sz, p.pos.y, 0, 0, 0, a,
        };
        transforms.push_back(m);
      }
      if (!transforms.empty())
        DrawMeshInstanced(ctx.mdlParticle.meshes[0],
                          ctx.mdlParticle.materials[0], &transforms[0],
                          transforms.size());
    }
    EndMode3D();
    if (car.playerIndex) {
      const Player &p = ctx.players[*car.playerIndex];
      const int frames = p.startFrame ? ctx.frame - *p.startFrame : 0;
      const std::string currentChrono = StepToChrono(frames);
      const std::string allBestChrono = StepToChrono(p.bestChrono.value_or(0));
      const std::string myBestChrono = StepToChrono(ctx.bestChrono.value_or(0));
      int y = 0;
      y = MyDrawText(0, y, WHITE, 20, "cp: %d", p.lastCP);
      y = MyDrawText(0, y, WHITE, 20, "%s", allBestChrono.c_str());
      y = MyDrawText(0, y, WHITE, 20, "%s", myBestChrono.c_str());
      y = MyDrawText(0, y, WHITE, 20, "%s", currentChrono.c_str());
    }
  }
};

void RenderMinimap(Context &ctx, Vector2 center, float scale) {
  const Vector2 pos{ctx.W - 266.0f, 10.0f};
  DrawTextureEx(ctx.trackTex, pos, 0, 1 / 4.0f, {255, 255, 255, 128});
  for (const Car &car : ctx.cars) {
    const auto &bb = ctx.track.aabb;
    const float x = 256.0f * (car.data.pos.x - bb.x) / bb.width;
    const float y = 256.0f * (car.data.pos.y - bb.y) / bb.height;
    DrawRectangleRec({pos.x + x - 2, pos.y + y - 2, 5, 5}, BLACK);
    DrawRectangleRec({pos.x + x - 1, pos.y + y - 1, 3, 3}, GREEN);
  }
}

void Render_Main(Context &ctx) {
  int y = 100;
  y = MyDrawText(200, y, WHITE, 40, "press start");
}

void Render_PlayerSelect(Context &ctx) {
  int y = 100;
  for (size_t i = 0; i < ctx.players.size(); ++i) {
    const auto &p = ctx.players[i];
    const Color c = p.enabled ? WHITE : GRAY;
    y = MyDrawText(200, y, c, 40, "player");
  }
}

void Render_Race(Context &ctx) {
  std::vector<std::tuple<Car *, Rectangle>> views;
  std::pair<int, int> rtSize;

  const int rtoi = 1;
  const float rtof = rtoi;

  const int playerCount = ctx.cars.size();
  if (playerCount == 1) {
    rtSize = {
        GetScreenWidth(),
        GetScreenHeight(),
    };
    views.emplace_back(&ctx.cars[0], Rectangle{
                                         0.0f,
                                         0.0f,
                                         float(GetScreenWidth()),
                                         float(GetScreenHeight()),
                                     });
  } else if (playerCount == 2) {
    rtSize = {
        GetScreenWidth() / 2 - 2 * rtoi,
        GetScreenHeight(),
    };
    views.emplace_back(&ctx.cars[0], Rectangle{
                                         rtof,
                                         0.0f,
                                         GetScreenWidth() / 2.0f - 2 * rtof,
                                         float(GetScreenHeight()),
                                     });
    views.emplace_back(&ctx.cars[1], Rectangle{
                                         rtof + GetScreenWidth() / 2.0f,
                                         0.0f,
                                         GetScreenWidth() / 2.0f - 2 * rtof,
                                         float(GetScreenHeight()),
                                     });
  } else if (playerCount == 4) {
    rtSize = {
        GetScreenWidth() / 2 - 2 * rtoi,
        GetScreenHeight() / 2 - 2 * rtoi,
    };
    views.emplace_back(&ctx.cars[0], Rectangle{
                                         rtof,
                                         rtof,
                                         GetScreenWidth() / 2.0f - 2 * rtof,
                                         GetScreenHeight() / 2.0f - 2 * rtof,
                                     });
    views.emplace_back(&ctx.cars[1], Rectangle{
                                         rtof + GetScreenWidth() / 2.0f,
                                         rtof,
                                         GetScreenWidth() / 2.0f - 2 * rtof,
                                         GetScreenHeight() / 2.0f - 2 * rtof,
                                     });
    views.emplace_back(&ctx.cars[2], Rectangle{
                                         rtof,
                                         rtof + GetScreenHeight() / 2.0f,
                                         GetScreenWidth() / 2.0f - 2 * rtof,
                                         GetScreenHeight() / 2.0f - 2 * rtof,
                                     });
    views.emplace_back(&ctx.cars[3], Rectangle{
                                         rtof + GetScreenWidth() / 2.0f,
                                         rtof + GetScreenHeight() / 2.0f,
                                         GetScreenWidth() / 2.0f - 2 * rtof,
                                         GetScreenHeight() / 2.0f - 2 * rtof,
                                     });
  }

  int propCount{};

  {
    int rti{};
    for (const auto &i : views) {
      auto &crt = ctx.rts[rti++];
      UpdateRtSize(crt, rtSize.first, rtSize.second);
      const Vector2 dim{
          float(crt.texture.width),
          float(crt.texture.height),
      };
      BeginTextureMode(crt);
      View3D::RenderView(ctx, *std::get<Car *>(i), dim, propCount);
      EndTextureMode();
    }
  }

  {
    rlDisableColorBlend();
    int rti{};
    for (const auto &i : views) {
      const auto &crt = ctx.rts[rti++];
      const Vector2 dim{
          float(crt.texture.width),
          float(crt.texture.height),
      };
      const auto &rt = crt.texture;
      const Rectangle src{0.0f, 0.0f, dim.x, -dim.y};
      DrawTexturePro(rt, src, std::get<Rectangle>(i), {0, 0}, 0, WHITE);
    }
    rlDrawRenderBatchActive();
    rlEnableColorBlend();
  }

  RenderMinimap(ctx, {}, 1.0f);

  if (ctx.showDebug) {
    int y = 0;
    y = MyDrawText(0, y, WHITE, 20, "%d fps", GetFPS());
    y = MyDrawText(0, y, WHITE, 20, "%d props", propCount);
  }
}

void Render(Context &ctx) {
  using RenderFn = void(Context &);
  RenderFn *const r[int(State::Count)] = {
      Render_Main,
      Render_PlayerSelect,
      Render_Race,
  };
  ctx.frame += 1;
  BeginDrawing();
  ClearBackground(BLACK);
  r[int(ctx.state)](ctx);
  EndDrawing();
}
