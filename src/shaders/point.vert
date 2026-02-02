VARYING vec4 v_color;

void MAIN()
{
    v_color = COLOR;
    POSITION = MODELVIEWPROJECTION_MATRIX * vec4(VERTEX, 1.0);
    // Ustawienie wielkości punktu (działa w OpenGL/Vulkan)
    gl_PointSize = 4.0;
}
