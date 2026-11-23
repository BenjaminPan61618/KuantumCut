#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(binding = 1) uniform sampler2D source;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float brightness;
    float contrast;
    float saturation;
    float m00;
    float m01;
    float m02;
    float m10;
    float m11;
    float m12;
    float m20;
    float m21;
    float m22;
    float keepHue;
    float keepRange;
    float strength;
};

float hueOf(vec3 c, float chroma, float maximum)
{
    if (chroma < 1e-5)
        return 0.0;
    if (maximum == c.r)
        return fract((c.g - c.b) / chroma / 6.0);
    if (maximum == c.g)
        return ((c.b - c.r) / chroma + 2.0) / 6.0;
    return ((c.r - c.g) / chroma + 4.0) / 6.0;
}

void main()
{
    vec4 c = texture(source, qt_TexCoord0);

    // 单色滤镜：保留指定色相，其余转为灰度
    if (keepRange > 0.0) {
        vec3 color = clamp(c.rgb, 0.0, 1.0);
        float maximum = max(color.r, max(color.g, color.b));
        float minimum = min(color.r, min(color.g, color.b));
        float chroma = maximum - minimum;
        float hue = hueOf(color, chroma, maximum);
        float hueDistance = abs(hue - keepHue);
        hueDistance = min(hueDistance, 1.0 - hueDistance);
        float keep = 1.0 - smoothstep(keepRange * 0.6, keepRange, hueDistance);
        keep *= smoothstep(0.02, 0.22, chroma / max(maximum, 1e-5));
        float gray = (color.r + color.g + color.b) / 3.0;
        vec3 filtered = mix(vec3(gray), color, keep);
        fragColor = vec4(mix(color, filtered, strength), c.a) * qt_Opacity;
        return;
    }

    vec3 rgb = c.rgb;
    float luma = dot(rgb, vec3(0.299, 0.587, 0.114));
    rgb = mix(vec3(luma), rgb, saturation);
    rgb = (rgb - 0.5) * contrast + 0.5 + brightness;
    vec3 outputColor;
    outputColor.r = m00 * rgb.r + m01 * rgb.g + m02 * rgb.b;
    outputColor.g = m10 * rgb.r + m11 * rgb.g + m12 * rgb.b;
    outputColor.b = m20 * rgb.r + m21 * rgb.g + m22 * rgb.b;
    outputColor = clamp(outputColor, 0.0, 1.0);
    fragColor = vec4(mix(c.rgb, outputColor, strength), c.a) * qt_Opacity;
}
