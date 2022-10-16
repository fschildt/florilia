struct Gl_Render_Rect {
    u32 vertex_array;
    u32 vertex_buffer;
    u32 shader;
};

struct Gl_Render_Font {
    u32 vertex_array;
    u32 vertex_buffer;
    u32 index_buffer;
    u32 texture;
    u32 shader;
    stbtt_bakedchar baked_chars[96];
};

class Gl_Renderer : public Renderer {
private:
    Gl_Render_Rect m_render_rect;
    Gl_Render_Font m_render_font;

    f32 m_projection_stbtt[4][4];
    f32 m_projection[4][4];

    s32 m_width;
    s32 m_height;


public:
    Gl_Renderer();

public:
    bool init(f32 font_size, u8 *ttf_buff, const char *font_vs, const char *font_fs, const char *rect_vs, const char *rect_fs);

    void viewport(s32 x, s32 y, s32 width, s32 height) override;
    void draw_background(V3 color) override;
    void draw_rect(Rect rect, V3 color) override;
    void draw_text(const char *text, f32 x, f32 y) override;


private:
    bool get_functions();
    bool init_text_drawing(u8 *ttf_buff, const char *vs_src, const char *fs_src, f32 font_size);
    bool init_rect_drawing(const char *vs_src, const char *fs_src);

    bool create_shader(u32 *shader, const char *vs_src, const char *fs_src);
    bool compile_shader(u32 *id, const char *src, GLenum type);


    // updates
    void ortho(f32 mat44[4][4], f32 x0, f32 y0, f32 x1, f32 y1, f32 z0, f32 z1);
};

