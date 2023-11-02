
#include "game.hpp"

struct Voxel {
  int texId{};
  float x{}, y{}, w{}, h{};
  int depth{};
  float lwidth{};
};

constexpr Voxel vxCar00{0, 7, 10, 16, 7, 6, 32};
constexpr Voxel vxCar01{1, 12, 6, 7, 19, 5, 32};

template <typename R, typename T> R *AllocCopy(const std::vector<T> &v) {
  void *mem = MemAlloc(v.size() * sizeof(T));
  T *r = (T *)mem;
  std::copy(v.begin(), v.end(), r);
  return (R *)mem;
}

template <typename T>
std::pair<T, T> CatmullRomSpline(float u, const T &P0, const T &P1, const T &P2,
                                 const T &P3) {
  T point{};
  point = point + 0.5f * u * u * u * ((-1) * P0 + 3 * P1 - 3 * P2 + P3);
  point = point + 0.5f * u * u * (2 * P0 - 5 * P1 + 4 * P2 - P3);
  point = point + 0.5f * u * ((-1) * P0 + P2);
  point = point + P1;

  float t2 = u * u;
  T m0 = .5f * (P2 - P0);
  T m1 = .5f * (P3 - P1);
  T tangent = (t2 - u) * 6.0f * P1 + (3.0f * t2 - 4.0f * u + 1.0f) * m0 +
              (-6.0f * t2 + 6.0f * u) * P2 + (3.0f * t2 - 2.0f * u) * m1;

  return {point, Vector2Normalize(tangent)};
}

std::pair<Vector2, Vector2> GetSplinePos(float r, const Vector2 &p0,
                                         const Vector2 &p1, const Vector2 &p2,
                                         const Vector2 &p3) {
  const auto [p, d] = CatmullRomSpline(r, p0, p1, p2, p3);
  const Vector2 n{-d.y, d.x};
  return {p, n};
};

std::pair<Vector2, Vector2> GetSplineAndDir(const std::vector<Vector2> &points,
                                            float r) {
  const int sz = points.size();
  const float rsz = r * sz;
  const int id1 = int(rsz) % sz;
  const int id0 = (id1 + sz - 1) % sz;
  const int id2 = (id1 + 1) % sz;
  const int id3 = (id1 + 2) % sz;
  const float sr = rsz - int(rsz);
  const Vector2 &p0 = points[id0];
  const Vector2 &p1 = points[id1];
  const Vector2 &p2 = points[id2];
  const Vector2 &p3 = points[id3];
  return GetSplinePos(sr, p0, p1, p2, p3);
};

Vector2 GetSpline(const std::vector<Vector2> &points, float r, float s) {
  const auto [p, n] = GetSplineAndDir(points, r);
  return p + s * n;
}

Mesh MakeMesh(const std::vector<Vector3> &vertice,
              const std::vector<Vector2> &uvs,
              const std::vector<uint16_t> &indice) {
  Mesh mesh{};
  mesh.vertexCount = vertice.size();
  mesh.triangleCount = indice.size() / 3;
  mesh.vertices = AllocCopy<float>(vertice);
  mesh.texcoords = AllocCopy<float>(uvs);
  mesh.indices = AllocCopy<uint16_t>(indice);
  UploadMesh(&mesh, false);
  return mesh;
}

Model MakeTrackRoad(const std::vector<Vector2> &points, bool loop, int pcount,
                    Vector2 width, Rectangle *aabb) {

  float minX = points[0].x;
  float maxX = minX;
  float minY = points[0].y;
  float maxY = minY;
  auto push = [&](Vector2 pos) -> Vector3 {
    minX = std::min(pos.x, minX);
    maxX = std::max(pos.x, maxX);
    minY = std::min(pos.y, minY);
    maxY = std::max(pos.y, maxY);
    // return {pos.x, pos.y, 0.0f};
    return {pos.x, 0.0f, pos.y};
  };

  const float umax = 20.0f;

  std::vector<Vector3> vertice;
  std::vector<Vector2> uvs;
  std::vector<uint16_t> indice;
  const float N = GameScale * width.y;
  const float P = GameScale * width.x;
  for (int i = 0; i < pcount; ++i) {
    const uint16_t p = vertice.size();
    const float r = i / float(pcount);
    vertice.push_back(push(GetSpline(points, r, N)));
    vertice.push_back(push(GetSpline(points, r, P)));
    const float u = i * umax / pcount;
    uvs.push_back({u, 0.0f});
    uvs.push_back({u, 1.0f});
    const std::array<uint16_t, 6> faces = {
        uint16_t(p + 0),  uint16_t(p + 2u), uint16_t(p + 1u),
        uint16_t(p + 2u), uint16_t(p + 3u), uint16_t(p + 1u),
    };
    indice.insert(indice.end(), faces.begin(), faces.end());
  }

  *aabb = {minX, minY, maxX - minX, maxY - minY};

  vertice.push_back(vertice[0]);
  vertice.push_back(vertice[1]);
  uvs.push_back({umax, 0.0f});
  uvs.push_back({umax, 1.0f});

  Mesh mesh = MakeMesh(vertice, uvs, indice);
  return LoadModelFromMesh(mesh);
}

std::vector<Vector2> MakeRandomTrack() {
  std::vector<Vector2> track;
  const int count = 17;
  for (int i = 0; i < count; ++i) {
    const float da = GetRandomValue(-100, 100) / 3000.0f;
    const float a = (i - da) * 2.0f * PI / count;
    const float r = 350.0f + GetRandomValue(0, 160);
    const float x = 10 * r * cosf(a);
    const float y = 10 * r * sinf(a);
    track.push_back(GameScale * Vector2{x, y});
  }
  return track;
}

Track MakeTrack(Context &ctx, const std::vector<Vector2> &track) {
  Track r{.track = track};
  Rectangle aabb{};

  {
    Texture t = LoadTexture("assets/road.png");
    GenTextureMipmaps(&t);
    SetTextureFilter(t, TEXTURE_FILTER_BILINEAR);

    {
      auto &mdl = r.models.emplace_back();
      mdl = MakeTrackRoad(track, false, 180, {-250.0f, 250.0f}, &aabb);
      mdl.materials[0].maps[MATERIAL_MAP_ALBEDO].texture = t;
    }
  }

  const auto DropShit = [&](int pcount, float o, float scale) {
    for (int i = 0; i < pcount; ++i) {
      const float p = i / float(pcount);
      Vector2 pos{};
      pos = GetSpline(track, p, o);
      r.props.push_back({{pos.x, 0, pos.y}, scale, &ctx.mdlTree});
    }
  };

  for (int i = 0; i < 8; ++i) {
    const float rt = i / 8.0f;
    const auto [p, n] = GetSplineAndDir(track, rt);
    r.checkpoints.push_back({p, 25.0f * n});
  }

  DropShit(45, 40.0f, 3.0f);
  DropShit(40, -40.0f, 3.0f);
  DropShit(25, 60.0f, 6.0f);
  DropShit(30, -60.0f, 6.0f);
  DropShit(10, 120.0f, 10.0f);
  DropShit(15, -120.0f, 10.0f);

  if (aabb.width < aabb.height) {
    aabb.x -= 0.5f * (aabb.height - aabb.width);
    aabb.width = aabb.height;
  } else {
    aabb.y -= 0.5f * (aabb.width - aabb.height);
    aabb.height = aabb.width;
  }

  r.aabb = aabb;
  return r;
}

Model ModelVoxels(Vector3 bsize, const std::vector<Texture> &vt,
                  const Voxel &vx, bool xySwap) {
  Model mdl{};

  const auto &t = vt[vx.texId];

  std::vector<Vector3> vertice;
  std::vector<Vector2> uvs;
  std::vector<uint16_t> indice;

  const auto push3 = [&](Vector3 p) -> Vector3 { return p; };
  const auto push2 = [&](Vector2 p) -> Vector2 { return p; };

  const Vector3 size = bsize;
  const int tw = t.width;
  const int th = t.height;
  const float dh = GameScale * 10.0f;
  const float v0 = (vx.y) / float(th);
  const float v1 = (vx.y + vx.h) / float(th);
  const float dx = 0.3f * size.x;
  const float dy = 0.0f;

  for (int i = 0; i < vx.depth; ++i) {
    const float u0 = (vx.x + i * vx.lwidth) / float(tw);
    const float u1 = (vx.x + i * vx.lwidth + vx.w) / float(tw);
    const uint16_t p = i * 4;
    vertice.push_back(push3({-size.x - dx, 1.0f + dh * i, size.y - dy}));
    vertice.push_back(push3({size.x - dx, 1.0f + dh * i, size.y - dy}));
    vertice.push_back(push3({size.x - dx, 1.0f + dh * i, -size.y - dy}));
    vertice.push_back(push3({-size.x - dx, 1.0f + dh * i, -size.y - dy}));
    if (xySwap) {
      uvs.push_back(push2({u0, v0}));
      uvs.push_back(push2({u0, v1}));
      uvs.push_back(push2({u1, v1}));
      uvs.push_back(push2({u1, v0}));
    } else {
      uvs.push_back(push2({u0, v1}));
      uvs.push_back(push2({u1, v1}));
      uvs.push_back(push2({u1, v0}));
      uvs.push_back(push2({u0, v0}));
    }
    indice.push_back(p + 0);
    indice.push_back(p + 1);
    indice.push_back(p + 2);
    indice.push_back(p + 0);
    indice.push_back(p + 2);
    indice.push_back(p + 3);
  }

  Mesh mesh = MakeMesh(vertice, uvs, indice);

  mdl = LoadModelFromMesh(mesh);
  mdl.materials[0].maps[MATERIAL_MAP_ALBEDO].texture = t;
  return mdl;
}

void InitCars(Context &ctx) {
  ctx.cars.clear();

  const auto [p, n] = GetSplineAndDir(ctx.track.track, 0.95f);
  for (size_t i = 0; i < ctx.players.size(); ++i) {
    const auto &player = ctx.players[i];
    if (player.enabled) {
      Car &car = ctx.cars.emplace_back();
      car.playerIndex = int(i);
      car.data.pos = p;
      car.model = i % 2;
      car.data.dir = atan2f(-n.x, n.y);
    }
  }
}

void Init(Context &ctx, int argc, char **argv) {
#ifndef DEBUG
  // SetTraceLogLevel(LOG_WARNING);
#endif
  InitWindow(ctx.W, ctx.H, "main");
  // SetWindowState(FLAG_FULLSCREEN_MODE);
  SetWindowState(FLAG_VSYNC_HINT);
  SetTargetFPS(60);
  DisableCursor();

  ctx.rts = {
      LoadRenderTexture(8, 8),
      LoadRenderTexture(8, 8),
      LoadRenderTexture(8, 8),
      LoadRenderTexture(8, 8),
  };
  ctx.voxelTex = {
      LoadTexture("assets/car00.png"),
      LoadTexture("assets/car01.png"),
  };
  ctx.noiseTex = LoadTexture("assets/noise00.png");
  GenTextureMipmaps(&ctx.noiseTex);
  SetTextureFilter(ctx.noiseTex, TEXTURE_FILTER_BILINEAR);

  ctx.mdlTree = LoadModel("assets/tree00.gltf");

  ctx.track = MakeTrack(ctx, MakeRandomTrack());
  ctx.checkPointsChrono.resize(ctx.track.checkpoints.size());

  ctx.shdGround = LoadShader("assets/shaders/ground.vs.glsl",
                             "assets/shaders/ground.fs.glsl");
  ctx.shdInstancing = LoadShader("assets/shaders/instancing.vs.glsl", nullptr);
  ctx.shdInstancing.locs[SHADER_LOC_MATRIX_MODEL] =
      GetShaderLocationAttrib(ctx.shdInstancing, "instanceTransform");

  ctx.mdlParticle = LoadModelFromMesh(GenMeshSphere(1, 8, 8));
  ctx.mdlParticle.materials[0].shader = ctx.shdInstancing;

  const Vector3 carSize = GameScale * Vector3{60, 30, 40};
  ctx.mdlCars = {
      ModelVoxels(carSize, ctx.voxelTex, vxCar00, false),
      ModelVoxels(carSize, ctx.voxelTex, vxCar01, true),
  };

  ctx.mdlGround = LoadModelFromMesh(GenMeshPlane(20000, 20000, 32, 32));
  ctx.mdlGround.materials[0].shader = ctx.shdGround;
  ctx.mdlGround.materials[0].maps[MATERIAL_MAP_ALBEDO].texture = ctx.noiseTex;
  ctx.mdlGround.materials[0].maps[MATERIAL_MAP_ALBEDO].color = WHITE;

  {
    const auto aabb = ctx.track.aabb;
    const int tsz = 1024;
    TraceLog(LOG_INFO, "aabb: %.01f %.01f %.01f %.01f", aabb.x, aabb.y,
             aabb.width, aabb.height);
    RenderTexture t = LoadRenderTexture(tsz, tsz);
    BeginTextureMode(t);
    ClearBackground({0, 0, 0, 64});
    {
      const float sz = aabb.width;
      const float x = aabb.x + 0.5f * sz;
      const float y = aabb.y + 0.5f * sz;
      const float scale = sz;
      const Camera3D cam{
          {x, 10, y}, {x, -10, y}, {0, 0, -1}, scale, CAMERA_ORTHOGRAPHIC,
      };
      BeginMode3D(cam);
      for (const auto &mdl : ctx.track.models)
        DrawModel(mdl, {}, 1, WHITE);
      EndMode2D();
    }
    EndTextureMode();
    Image img = LoadImageFromTexture(t.texture);
    UnloadRenderTexture(t);
    ImageFlipVertical(&img);
    TraceLog(LOG_INFO, "image: %dx%d", img.width, img.height);
    ctx.trackTex = LoadTextureFromImage(img);
    UnloadImage(img);
  }
}
