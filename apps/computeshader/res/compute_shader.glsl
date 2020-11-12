#version 430
layout(local_size_x = 1) in;

layout(rgba32f, binding = 0) uniform image2D img_output;

layout(std430, binding = 1) buffer Map
{
  int width;
  int height;
  int tiles[];
} map;

layout(std430, binding = 2) buffer ViewParams
{
  int view_width;
  int view_height;
  vec2 viewer_pos;
  float viewer_angle;
  float fov_y;
} view_params;

#define M_PI 3.1415926535897932384626433832795
#define FLT_MAX 3.402823466e+38

void main()
{
  int column = int(gl_GlobalInvocationID.x);
  float screen_aspect = float(view_params.view_width) / float(view_params.view_height);
  float half_fov = view_params.fov_y * 0.5;
  float view_distance = screen_aspect / tan(half_fov);

  // Get world space ray direction for column
  vec2 ray_dir_a = vec2(view_distance, screen_aspect * (float(column) - float(view_params.view_width) * 0.5) / float(view_params.view_width));
  ray_dir_a = normalize(ray_dir_a);
  mat2 rot = mat2(
    cos(view_params.viewer_angle), -sin(view_params.viewer_angle),
    sin(view_params.viewer_angle), cos(view_params.viewer_angle)
  );
  vec2 ray_dir = rot * ray_dir_a;

  // Cast ray through the map and find the nearest intersection with a solid tile
  float distance = FLT_MAX;

  if (ray_dir.x > 0.0)
  {
      float dydx = ray_dir.y / ray_dir.x;
      int tx = int(floor(view_params.viewer_pos.x + 1.0));

      for ( ; tx < map.width; ++tx)
      {
          float y = view_params.viewer_pos.y + dydx * (tx - view_params.viewer_pos.x);
          int ty = int(floor(y));

          if (ty < 0 || ty >= map.height)
          {
              break;
          }

          if (map.tiles[ty * map.width + tx] != 0)
          {
              float hit_d = (tx - view_params.viewer_pos.x) * (tx - view_params.viewer_pos.x) + (y - view_params.viewer_pos.y) * (y - view_params.viewer_pos.y);

              if (hit_d < distance)
              {
                  distance = hit_d;
                  //column = (int)(y * 64.0f + 0.5f) & 63;
              }

              break;
          }
      }
  }
  else if (ray_dir.x < 0.0)
  {
      float dydx = ray_dir.y / ray_dir.x;
      int tx = int(floor(view_params.viewer_pos.x - 1.0));

      for ( ; tx >= 0; --tx)
      {
          float y = view_params.viewer_pos.y + dydx * (tx + 1.0 - view_params.viewer_pos.x);
          int ty = int(floor(y));

          if (ty < 0 || ty >= map.height)
          {
              break;
          }

          if (map.tiles[ty * map.width + tx] != 0)
          {
              float hit_d = (tx + 1.0 - view_params.viewer_pos.x) * (tx + 1.0 - view_params.viewer_pos.x) + (y - view_params.viewer_pos.y) * (y - view_params.viewer_pos.y);

              if (hit_d < distance)
              {
                  distance = hit_d;
                  //column = (int)(y * 64.0f + 0.5f) & 63;
              }

              break;
          }
      }
  }

  if (ray_dir.y > 0.0)
  {
      float dxdy = ray_dir.x / ray_dir.y;
      int ty = int(floor(view_params.viewer_pos.y + 1.0));

      for ( ; ty < map.height; ++ty)
      {
          float x = view_params.viewer_pos.x + dxdy * (ty - view_params.viewer_pos.y);
          int tx = int(floor(x));

          if (tx < 0 || tx >= map.width)
          {
              break;
          }

          if (map.tiles[ty * map.height + tx] != 0)
          {
              float hit_d = (x - view_params.viewer_pos.x) * (x - view_params.viewer_pos.x) + (ty - view_params.viewer_pos.y) * (ty - view_params.viewer_pos.y);

              if (hit_d < distance)
              {
                  distance = hit_d;
                  //column = (int)(x * 64.0f + 0.5f) & 63;
              }

              break;
          }
      }
  }
  else if (ray_dir.y < 0.0)
  {
      float dxdy = ray_dir.x / ray_dir.y;
      int ty = int(floor(view_params.viewer_pos.y - 1.0));

      for ( ; ty >= 0; --ty)
      {
          float x = view_params.viewer_pos.x + dxdy * (ty + 1.0 - view_params.viewer_pos.y);
          int tx = int(floor(x));

          if (tx < 0 || tx >= map.width)
          {
              break;
          }

          if (map.tiles[ty * map.width + tx] != 0)
          {
              float hit_d = (x - view_params.viewer_pos.x) * (x - view_params.viewer_pos.x) + (ty + 1.0 - view_params.viewer_pos.y) * (ty + 1.0 - view_params.viewer_pos.y);

              if (hit_d < distance)
              {
                  distance = hit_d;
                  //column = (int)(x * 64.0f + 0.5f) & 63;
              }

              break;
          }
      }
  }

/*
  if (distance == FLT_MAX)
  {
      // Ray hit nothing
      for (int y = 0; y < screen_height / 2; ++y)
      {
          distance = (screen_height * wall_height * screen_distance) / (screen_height - 2.0f * y);
          float attenuation_factor = 6.0f / distance;
          attenuation_factor = attenuation_factor < 0.0f ? 0.0f : (attenuation_factor > 1.0f ? 1.0f : attenuation_factor);
          set_pixel(col, y, attenuate(sky_color, attenuation_factor));
          set_pixel(col, screen_height - y - 1, attenuate(floor_color, attenuation_factor));
      }
  }
  else
  {
      // Project hit distance to view depth
      distance = sqrtf(distance) * screen_rays[col].x;

      float attenuation_factor = 6.0f / distance;
      attenuation_factor = attenuation_factor < 0.0f ? 0.0f : (attenuation_factor > 1.0f ? 1.0f : attenuation_factor);

      // Scale wall column height by distance
      float column_height = wall_height * (screen_distance / distance);
      int ceiling = (int)((1.0f - column_height) * 0.5f * screen_height);
      int floor = screen_height - ceiling;

      for (int y = 0; y < screen_height / 2 && y < ceiling; ++y)
      {
          distance = (screen_height * wall_height * screen_distance) / (screen_height - 2.0f * y);
          float attenuation_factor = 6.0f / distance;
          attenuation_factor = attenuation_factor < 0.0f ? 0.0f : (attenuation_factor > 1.0f ? 1.0f : attenuation_factor);
          set_pixel(col, y, attenuate(sky_color, attenuation_factor));
          set_pixel(col, screen_height - y - 1, attenuate(floor_color, attenuation_factor));
      }

      float v = 0.0f;
      float dvdy = 64.0f / (floor - ceiling);

      for (int y = ceiling; y < screen_height && y < floor; ++y)
      {
          int iv = (int)(v + 0.5f) & 63;
          uint8_t texel = wall_texture[iv * 64 + column];
          set_pixel(col, y, attenuate(texel, attenuation_factor));
          v += dvdy;
      }
  }
*/

  if (distance == FLT_MAX)
  {
    // No hit
    for (int y = 0; y < view_params.view_height; ++y)
    {
      ivec2 pixel_coords = ivec2(gl_GlobalInvocationID.x, y);
      imageStore(img_output, pixel_coords, vec4(0, 0, 0, 1));
    }
  }
  else
  {
    distance = sqrt(distance) * ray_dir_a.x;
    int column_height = int(128.0 * view_distance / distance);
    int top = (view_params.view_height - column_height) / 2;
    int bottom = top + column_height;

    if (top < 0) top = 0;
    if (bottom > view_params.view_height) bottom = view_params.view_height;

    for (int y = 0; y < top; ++y)
    {
      ivec2 pixel_coords = ivec2(gl_GlobalInvocationID.x, y);
      imageStore(img_output, pixel_coords, vec4(0, 0, 0, 1));
    }

    for (int y = top; y < bottom; ++y)
    {
      ivec2 pixel_coords = ivec2(gl_GlobalInvocationID.x, y);
      imageStore(img_output, pixel_coords, vec4(0, 0, 0.7, 1));
    }

    for (int y = bottom; y < view_params.view_height; ++y)
    {
      ivec2 pixel_coords = ivec2(gl_GlobalInvocationID.x, y);
      imageStore(img_output, pixel_coords, vec4(0, 0, 0, 1));
    }
  }
}
