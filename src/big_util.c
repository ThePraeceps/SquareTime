#include "big_util.h"
  
static void cl_on_animation_stopped(Animation *anim, bool finished, void *context)
{
property_animation_destroy((PropertyAnimation*) anim);
}
/*
* Animate a layer with duration and delay
*/
void cl_animate_layer(Layer *layer, GRect start, GRect finish, int duration, int delay)
{
PropertyAnimation *anim = property_animation_create_layer_frame(layer, &start, &finish);
animation_set_duration((Animation*) anim, duration);
animation_set_delay((Animation*) anim, delay);
AnimationHandlers handlers = {
.stopped = (AnimationStoppedHandler) cl_on_animation_stopped
};
animation_set_handlers((Animation*) anim, handlers, NULL);
animation_schedule((Animation*) anim);
}