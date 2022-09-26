/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2021 vishkumar <<user@hostname.org>>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:element-process
 *
 * FIXME:Describe process here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! process ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>

#include "gstprocess.h"

/* for GST_VIDEO_CAPS_MAKE to make video caps  */
#include <gst/video/video.h>

/* define debugging category (statically)
   set that category as default debugging strategy
*/
GST_DEBUG_CATEGORY_STATIC (gst_process_debug);
#define GST_CAT_DEFAULT gst_process_debug

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

/* Plugin properties  */ 
enum
{
  PROP_0,
  PROP_SILENT,
  PROP_BLINK,
  PROP_ROTATE,
};

/* globle variable for counting number of frames and doing blinking */ 
int frames=0;
int flag=-1;

/* Pad Template Definition
   used for defining the capabilities(supported formats) of the inputs and outputs.
   description of a pad that the element will (or might) create and use. it contains:
       - A short name for the pad.
       - Pad direction.
       - Existence property. This indicates whether the pad exists always (an “always” pad), only in some cases (a “sometimes” pad) or only if the application requested such a pad (a “request” pad).
       - Supported types/formats by this element (capabilities).
*/
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("{ NV12, YV12, I420, YUY2, RGBA }"))
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("{ NV12, YV12, I420, YUY2, RGBA }"))
    );

#define gst_process_parent_class parent_class
G_DEFINE_TYPE (Gstprocess, gst_process, GST_TYPE_ELEMENT);


static void gst_process_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_process_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static gboolean gst_process_sink_event (GstPad * pad,
    GstObject * parent, GstEvent * event);
static gboolean gst_process_sink_query (GstPad    *pad,
    GstObject *parent, GstQuery  *query);
static GstFlowReturn gst_process_chain (GstPad * pad,
    GstObject * parent, GstBuffer * buf);


/* Constructor Function for plugin
 * initialize the process's class 
 * common to all instances of the process's class
 * used to initialise the class only once
 * install plugin properties
 * set sink and src pad capabilities
 * override the required functions of the base class (GObject vmethod implementations)
*/
static void
gst_process_class_init (GstprocessClass * klass)
{
  g_print ("gst_process_class_init \n");
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_process_set_property;
  gobject_class->get_property = gst_process_get_property;

  g_object_class_install_property (gobject_class, PROP_SILENT,
      g_param_spec_boolean ("silent", "Silent", "Produce verbose output ?",
          FALSE, G_PARAM_READWRITE));
  
  /* Installing a new property named blink */ 
  g_object_class_install_property (gobject_class, PROP_BLINK,
      g_param_spec_uint ("blink", "Blink","blinking the farames", 0, G_MAXUINT,
          0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));  

  /* Installing a new property named rotate */ 
  g_object_class_install_property (gobject_class, PROP_ROTATE,
      g_param_spec_uint ("rotate", "Rotate","Rotate frame by 180 degree", 0, G_MAXUINT,
          0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)); 

  /* Adding element metadata which provides extra element information.*/
  gst_element_class_set_details_simple (gstelement_class,
      "process",
      "process",
      "Process the frames if incoming stream", "vishkumar <<vishkumar@nvidia.com>>");

  /* Registering the Pad Template with element class
     e.i. adding a padtemplate to an element class */
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_factory));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_factory));
}

/* Constructor Function for plugin
 * initialise an instance of process's class
 * specific to an instance of process's class
 * instantiate pads and add them to element
 * set pad callback functions
 * initialize instance structure
 * initialize properties
 */
static void
gst_process_init (Gstprocess * filter)
{
  g_print ("gst_process_init \n");

  /* creating pad from the registerd pad templates, through this pad data comes in to the element  */
  filter->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");

  /* configure event function on the sinkpad before adding the pad to the element */
  gst_pad_set_event_function (filter->sinkpad,
      GST_DEBUG_FUNCPTR (gst_process_sink_event));
  
  /* Sets the given chain function for the pad. The chain function is called to process a GstBuffer input buffer */
  gst_pad_set_chain_function (filter->sinkpad,
      GST_DEBUG_FUNCPTR (gst_process_chain));

  /* configure qyery function on the sinkpad before adding the pad to the element */
  gst_pad_set_query_function (filter->sinkpad,
      GST_DEBUG_FUNCPTR (gst_process_sink_query));

  /* Set pad to proxy caps, so that all caps-related events and queries are proxied downstream or upstream to the other side of the element automatically. 
     Set this if the element always outputs data in the exact same format as it receives as input. 
     This is just for convenience to avoid implementing some standard event and query handling code in an element. 
  */ 
  GST_PAD_SET_PROXY_CAPS (filter->sinkpad);

  /* Adds a pad (link point) to element. This function will emit the pad-added signal on the element. */ 
  gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad);

  /* creating pad from the registerd static templates,through this pad data goes out of the element  */
  filter->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  GST_PAD_SET_PROXY_CAPS (filter->srcpad); 
  gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);

  /* initialize properties */
  filter->silent = FALSE;
  filter->blink = 0;
  filter->rotate = 0;
}

/* this function is called when a property of the plugin is set*/ 
static void
gst_process_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  g_print ("gst_process_set_property %d \n",prop_id);
  
  Gstprocess *filter = GST_PROCESS (object);

  switch (prop_id) {
    case PROP_SILENT:
      filter->silent = g_value_get_boolean (value);
      break;
    case PROP_BLINK:
      filter->blink = g_value_get_uint (value);
      break;
    case PROP_ROTATE:
      filter->rotate = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* this Function is when a property of the plugin is requested */ 
static void
gst_process_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  g_print ("gst_process_get_property %d \n",prop_id);

  Gstprocess *filter = GST_PROCESS (object);

  switch (prop_id) {
    case PROP_SILENT:
      g_value_set_boolean (value, filter->silent);
      break;
    case PROP_BLINK:
      g_value_set_uint (value, filter->blink);
      break; 
    case PROP_ROTATE:
      g_value_set_uint (value, filter->rotate);
      break; 
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}


/* GstElement vmethod implementations */
/* this function handles sink events */
static gboolean
gst_process_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  g_print ("gst_process_sink_event recieved %s event at %s pad\n",GST_EVENT_TYPE_NAME (event),GST_PAD_NAME (pad));

  Gstprocess *filter;
  gboolean ret;

  filter = GST_PROCESS (parent);

  GST_LOG_OBJECT (filter, "Received %s event: %" GST_PTR_FORMAT,
      GST_EVENT_TYPE_NAME (event), event);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      /* we should handle the format here if needed */

      /* Get the caps from event */
      GstCaps *caps;
      gst_event_parse_caps (event, &caps);
      
      /* Printing the sinkpad caps */
      g_print("GST_EVENT_CAPS %s \n",gst_caps_to_string(caps));
      
      /* push the event downstream */
      ret = gst_pad_push_event (filter->srcpad, event);
      break;
    }
    default:
      ret = gst_pad_event_default (pad, parent, event);
      break;
  }
  return ret;
}


static gboolean
gst_process_sink_query (GstPad    *pad, GstObject *parent,
    GstQuery  *query)
{
  g_print ("gst_process_sink_query recieved %s query at %s pad\n",GST_QUERY_TYPE_NAME (query),GST_PAD_NAME (pad));
  gboolean ret;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_POSITION:
    {
      /* we should report the current position */
      ret = gst_pad_query_default (pad, parent, query);
      break;
    }
    case GST_QUERY_DURATION:
    {
      /* we should report the duration here */
      ret = gst_pad_query_default (pad, parent, query);
      break;
    }
    case GST_QUERY_CAPS:
    {
      /* we should report the supported caps here */
      GstCaps *caps;
      /* Get the caps from query */
      gst_query_parse_caps (query, &caps);

      /* Printing the sinkpad caps */
      g_print("GST_QUERY_CAPS %s \n",gst_caps_to_string(caps));
      
      ret = gst_pad_query_default (pad, parent, query);
      break;
    }
    case GST_QUERY_ACCEPT_CAPS:
    {
      /* we should report the supported caps here */
      GstCaps *caps;
      /* Get the caps from query */
      gst_query_parse_accept_caps (query, &caps);

      /* Printing the sinkpad caps */
      g_print("GST_QUERY_ACCEPT_CAPS %s \n",gst_caps_to_string(caps));
      
      ret = gst_pad_query_default (pad, parent, query);
      break;
    }
    default:
      /* just call the default handler */
      ret = gst_pad_query_default (pad, parent, query);
      break;
  }
  return ret;
}


/* chain function -> this function does the actual processing
   A function that will be called on sinkpads when chaining buffers. 
   The function typically processes the data contained in the buffer and 
   either consumes the data or passes it on to the internally linked pad(s). 
*/ 
static GstFlowReturn
gst_process_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
  g_print ("gst_process_chain \n");
  Gstprocess *filter;

  filter = GST_PROCESS (parent);
  
  GstMapInfo map;
  int dataLength;
  guint8 *data;
    
  gst_buffer_map(buf, &map, GST_MAP_WRITE); 
  dataLength = map.size;
  data = map.data;
  frames++;
  
  /* blink the frames */
  if(filter->blink){
    // flag -1 disable the scren(blank/green screen)
    if(flag>0){
      for (int i=0; i <= dataLength/2; i++) {
        data[i] = 0;
      }
    }
    // after 10 frames flip the flag for blink
    if(frames%10==0){
      flag=flag*-1;
    }
  }
  
  /* rotate frame by 180 degree*/
  if(filter->rotate){
    for (int i=0; i <= dataLength/2; i++) {
      guint8 t=data[dataLength-i];
      data[dataLength-i]=data[i];
      data[i] = t;
    }   
  }

  if (filter->silent == FALSE)
    g_print ("frame no - %d \n",frames);
  
  gst_buffer_unmap (buf, &map);


  /* push out the incoming buffer to the srcpad */ 
  return gst_pad_push (filter->srcpad, buf);
}


/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 * this is called as soon as the plugin is loaded, and should return TRUE or FALSE 
 * depending on whether it loaded initialized any dependencies correctly
 */
static gboolean
process_init (GstPlugin * process)
{
  g_print ("process_init \n");

  /* debug category for filtering log messages*/
  GST_DEBUG_CATEGORY_INIT (gst_process_debug, "process",
      0, "Process Plugin");

  return gst_element_register (process, "process", GST_RANK_NONE,
      GST_TYPE_PROCESS);
}

/* PACKAGE: this is usually set by meson depending on some _INIT macro
 * in meson.build and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use meson to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */
#ifndef PACKAGE
#define PACKAGE "myfirstprocess"
#endif

/* gstreamer looks for this structure to register processs */
GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    process,
    "Process Plugin",
    process_init,
    PACKAGE_VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
