uniform vec2 rot; // rot.xy = {cos(theta), sin(theta)}
attribute vec2 pos_attr;
attribute vec4 col_attr;
varying vec4 col_var;
void main()
{
    gl_Position.x = rot.x * pos_attr.x - rot.y * pos_attr.y;
    gl_Position.y = rot.y * pos_attr.x + rot.x * pos_attr.y;
    gl_Position.z = 0.0;
    col_var = col_attr;
}