
#include "game.hpp"

void Release(Context &ctx) {}

int main(int argc, char **argv) {
  Context ctx;
  Init(ctx, argc, argv);
  while (Update(ctx))
    Render(ctx);
  Release(ctx);
  return 0;
}
