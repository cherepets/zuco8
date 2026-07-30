attribute vec2 pos_attr;
attribute vec2 uv_attr;
varying vec2 uv_var;

void main()
{
    gl_Position = vec4(pos_attr, 0.0, 1.0);
    uv_var = uv_attr;
}
