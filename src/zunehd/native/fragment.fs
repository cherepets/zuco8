#pragma profilepragma blendoperation( gl_FragColor, GL_FUNC_ADD, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_FUNC_ADD, GL_ONE, GL_ONE)

uniform sampler2D texture_sampler;
varying vec2 uv_var;

void main()
{
    gl_FragColor = texture2D(texture_sampler, uv_var);
}
