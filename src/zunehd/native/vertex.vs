attribute vec2 pos_attr;
attribute vec2 uv_attr;
varying vec2 uv_var;
uniform vec3 rotation_transform;
uniform vec2 rotation_pivot;

void main()
{
    vec2 offset = pos_attr - rotation_pivot;
    vec2 rotated = vec2(
        offset.x * rotation_transform.x - offset.y * rotation_transform.y,
        offset.x * rotation_transform.z + offset.y * rotation_transform.x);
    gl_Position = vec4(rotated + rotation_pivot, 0.0, 1.0);
    uv_var = uv_attr;
}
