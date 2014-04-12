/*

   The original source image is from:

      <http://openclipart.org/detail/26728/aiga-litter-disposal-by-anonymous>

   The source image was converted from an SVG into a RGB bitmap using
   Inkscape. It has no transparency and uses only black and white as
   colors.

*/

#include "pebble.h"
#include "picojpeg.h"
  
#include "sample_image_data.h"

static Window *window;
static Layer *layer;
static GBitmap *image;
static unsigned char threshold = 128;

extern GBitmap g_Bmp;



extern void init_bitmap();
extern unsigned char *pjpeg_load_from_data(const unsigned char *pImgData, int nImgDataSize, int *x, int *y, int *comps, pjpeg_scan_type_t *pScan_type, int reduce, unsigned char bw_threshold);
  

static void prepare_jpeg()
{
   int width, height, comps;
   pjpeg_scan_type_t scan_type;
   unsigned char *pImage = 0;
   int reduce = 0;

  init_bitmap();
  pImage = pjpeg_load_from_data(g_Image, sizeof(g_Image), &width, &height, &comps, &scan_type, reduce, threshold);
  if (!pImage)
   {
   		//Error of JPEG decoding
	    //return EXIT_FAILURE;
   }
  image = &g_Bmp;
}


static void layer_update_callback(Layer *me, GContext* ctx) {
  // We make sure the dimensions of the GRect to draw into
  // are equal to the size of the bitmap--otherwise the image
  // will automatically tile. Which might be what *you* want.

  GRect bounds = image->bounds;
  graphics_draw_bitmap_in_rect(ctx, image, (GRect) { .origin = { 0, 0 }, .size = bounds.size });
  
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (threshold < 255) {
    threshold += 10;
    prepare_jpeg();
  }
  layer_mark_dirty(layer);
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (threshold > 0) {
    threshold -= 10;
    prepare_jpeg();
  }
  layer_mark_dirty(layer);
}

static void config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}

int main(void) {
  
  
  window = window_create();
  window_stack_push(window, true /* Animated */);

  window_set_click_config_provider(window, config_provider);

  // Init the layer for display the image
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_frame(window_layer);
  layer = layer_create(bounds);
  layer_set_update_proc(layer, layer_update_callback);
  layer_add_child(window_layer, layer);


  prepare_jpeg();

  app_event_loop();

  window_destroy(window);
  layer_destroy(layer);
}
