extern int audio;
extern int audio_rate;
extern int audio_bufsz;
extern int sintab[] ;
extern Clock audio_clock;
extern void audio_add(AudioHandler *fn, void *vp) ;
extern int audio_del(AudioHandler *fn, void *vp) ;
extern int handle_audio_setup(Parse *pp) ;
extern void clock_setup(Clock *ck, double rate, int now) ;
extern int clock_inc(Clock *ck, int now) ;
extern int colour_data[];
extern inline void sincos_init(double *buf, double freq) ;
extern inline void sincos_step(double *buf) ;
extern int line_error(Parse *pp, char *p, char *fmt, ...) ;
extern int load_config(void *vp) ;
extern int handle_sect(Parse *pp) ;
extern int parse(Parse *pp, char *fmt0, ...) ;
extern int parseEOF(Parse *pp) ;
extern int handle_page_setup(Parse *pp, int fn) ;
extern Device *dev;
extern int handle_dev_setup(Parse *pp) ;
extern int serial_thread(void *vp) ;
extern int file_thread(void *vp) ;
extern void serial_audio_callback(int now) ;
extern void modEEGold_handler() ;
extern void modEEG_handler() ;
extern void jm_handler() ;
extern SDL_Surface *disp;
extern Uint32 *disp_pix32;
extern Uint16 *disp_pix16;
extern int disp_my;
extern int disp_sx, disp_sy;
extern int disp_rl, disp_rs;
extern int disp_gl, disp_gs;
extern int disp_bl, disp_bs;
extern int disp_am;
extern int *colour;
extern Page *page;
extern int tick_inc;
extern int tick_targ;
extern Uint32 main_threadid;
extern int server;
extern int server_rd;
extern int server_out;
extern double nan_global;
extern Page *p_fn[] ;
extern void usage() ;
extern int main(int ac, char **av) ;
extern int setup_server_connection(char *serv, int port, Parse *pp) ;
extern void nsd_handler() ;
extern void nsd_line(char *line) ;
extern void server_loop() ;
extern void send_edf_header(int fd) ;
extern void server_handler() ;
extern Page * p_audio_init(Parse *pp) ;
extern Page * p_bands_init(Parse *pp) ;
extern Page * p_bands_init(Parse *pp) ;
extern int applog_force_update;
extern int applog(char *fmt, ...) ;
extern Page * p_console_init() ;
extern Page * p_timing_init(Parse *pp) ;
extern Settings * set_new(char *spec) ;
extern void set_delete(Settings *ss) ;
extern void set_load_presets(Settings *ss, char *txt) ;
extern int set_rearrange(Settings *ss, short *font, int sx) ;
extern void set_position(Settings *ss, int ox, int oy) ;
extern void set_draw(Settings *ss) ;
extern int set_event(Settings *ss, Event *ev) ;
extern short font6x8[];
extern short font6x12[];
extern short font8x16[];
extern short font10x20[];
extern int suspend_update;
extern void graphics_init(int sx, int sy, int bpp) ;
extern int map_rgb(int col) ;
extern int colour_mix(int prop, int col0, int col1) ;
extern void update(int xx, int yy, int sx, int sy) ;
extern void update_force(int xx, int yy, int sx, int sy) ;
extern void update_all() ;
extern void mouse_pointer(int on) ;
extern void clear_rect(int xx, int yy, int sx, int sy, int val) ;
extern void alpha_rect(int xx, int yy, int sx, int sy, int val, int opac) ;
extern void scroll(int xx, int yy, int sx, int sy, int ox, int oy) ;
extern void mapcol(int xx, int yy, int sx, int sy, int col0, int col1) ;
extern void vline(int xx, int yy, int sy, int val) ;
extern void hline(int xx, int yy, int sx, int val) ;
extern int get_point(int xx, int yy) ;
extern void plot(int xx, int yy, int val) ;
extern void drawtext(short *font, int xx, int yy, char *str) ;
extern void drawtext4(short *font, int xx, int yy, char *str) ;
extern int colstr_len(char *str) ;
extern int pure_hue_src[] [4];
extern Uint8 pure_hue_mem[] ;
extern Uint8 *pure_hue_data[] [3];
extern void init_pure_hues() ;
extern void plot_hue(int xx, int yy, int sy, double ii, double hh) ;
extern int cint_table[] ;
extern void init_cint_table() ;
extern void plot_cint(int xx, int yy, int sy, double ii) ;
extern void plot_cint_bar(int xx, int yy, int sx, int sy, int unit, double ii) ;
extern void plot_gray(int xx, int yy, int sy, double ii) ;
extern int main(int ac, char **av) ;
extern void error(char *fmt, ...) ;
extern int main(int ac, char **av) ;
extern int main(int ac, char **av) ;
extern void free_all_regions(Page *pg) ;
extern void free_my_regions(Page *pg, void *parent) ;
extern void add_keyregion(Page *pg, int x0, int x1, int y0, int y1, int col0, int col1, int col2, int sym, int mod, int uc) ;
extern void draw_status(short *font) ;
extern void status(char *fmt, ...) ;
extern void tick_timer(int ms) ;
extern void page_switch(Page *new_page) ;
extern char *scratch;
extern int scr_len;
extern int scr_max;
extern int scr_wid;
extern char *scr_ind;
extern int scr_indlen;
extern void scr_realloc() ;
extern void scr_zap() ;
extern void scr_wrap(int wid, char *ind) ;
extern void scr_vpr(char *fmt, va_list ap) ;
extern void scr_pr(char *fmt, ...) ;
extern void scr_prw(char *fmt, ...) ;
extern void scr_zap_pr(char *fmt, ...) ;
extern void scr_lf() ;
extern char * scr_dup() ;
extern void * scr_inc(int len) ;
extern void scr_wrD(double dval) ;
extern void scr_wrI(int ival) ;
extern StrStream * strstream_open(int maxsiz) ;
extern char * strstream_close(StrStream *ss) ;
extern int time_now_ms() ;
extern int time_now_ms() ;
extern void time_hhmmss(char *dst) ;
extern void time_hhmmss(char *dst) ;
extern void wrN(char **pp, int val) ;
extern void wrSN(char **pp, int val) ;
extern int rdN(char **pp) ;
extern int rdSN(char **pp) ;
extern int memhash(void *p0, void *p1) ;
extern int strhash(char *str) ;
extern void error(char *fmt, ...) ;
extern void errorSDL(char *fmt, ...) ;
extern void warn(char *fmt, ...) ;
extern void * Alloc(int size) ;
extern void * StrDup(char *str) ;
extern void * StrDupRange(char *str, char *end) ;
extern void * MemDup(void *p0, void *p1) ;
extern void XAlloc(void **var, int siz) ;
extern void XGrow(void **var, void *pos) ;