/*
    pygame - Python Game Library
    Copyright (C) 2000-2001  Pete Shinners

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    Pete Shinners
    pete@shinners.org
*/

/*
 *  mixer module for pygame
 */
#define PYGAMEAPI_MIXER_INTERNAL
#include "pygame.h"
#include "mixer.h"




staticforward PyTypeObject PySound_Type;
staticforward PyTypeObject PyChannel_Type;
static PyObject* PySound_New(Mix_Chunk*);
static PyObject* PyChannel_New(int);
#define PySound_Check(x) ((x)->ob_type == &PySound_Type)
#define PyChannel_Check(x) ((x)->ob_type == &PyChannel_Type)


static int request_frequency = MIX_DEFAULT_FREQUENCY;
static int request_size = MIX_DEFAULT_FORMAT;
static int request_stereo = 1;



static void autoquit(void)
{
	if(SDL_WasInit(SDL_INIT_AUDIO))
	{
		Mix_CloseAudio();
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
	}
}


static PyObject* autoinit(PyObject* self, PyObject* arg)
{
	int freq, size, stereo;
	freq = request_frequency;
	size = request_size;
	stereo = request_stereo;
	if(!PyArg_ParseTuple(arg, "|iii", &freq, &size, &stereo))
		return NULL;
	if(stereo)
		stereo = 2;
	else
		stereo = 1;

	if(!SDL_WasInit(SDL_INIT_AUDIO))
	{
		PyGame_RegisterQuit(autoquit);

		if(SDL_InitSubSystem(SDL_INIT_AUDIO) == -1)
			return PyInt_FromLong(0);

		if(Mix_OpenAudio(freq, MIX_DEFAULT_FORMAT, stereo, 1024) == -1)
		{
			SDL_QuitSubSystem(SDL_INIT_AUDIO);
			return PyInt_FromLong(0);
		}
	}
	return PyInt_FromLong(1);
}


    /*DOC*/ static char doc_quit[] =
    /*DOC*/    "pygame.mixer.quit() -> None\n"
    /*DOC*/    "unitializes the mixer\n"
    /*DOC*/    "\n"
    /*DOC*/    "This will stop all playing sounds and uninitialize\n"
    /*DOC*/    "the mixer module\n"
    /*DOC*/ ;

static PyObject* quit(PyObject* self, PyObject* arg)
{
	if(!PyArg_ParseTuple(arg, ""))
		return NULL;

	autoquit();

	RETURN_NONE
}


    /*DOC*/ static char doc_init[] =
    /*DOC*/    "pygame.mixer.init([freq, [size, [stereo]]]) -> None\n"
    /*DOC*/    "initialize mixer module\n"
    /*DOC*/    "\n"
    /*DOC*/    "Initializes the mixer module. Usually no arguments will be\n"
    /*DOC*/    "needed, the defaults are 22050 frequency data in stereo with\n"
    /*DOC*/    "signed 16bit data. The size argument can be 8 or 16 for unsigned\n"
    /*DOC*/    "data, or -8 or -16 for signed data.\n"
    /*DOC*/    "\n"
    /*DOC*/    "On many platforms it is important that the display module is\n"
    /*DOC*/    "initialized before the audio. (that is, if the display will be\n"
    /*DOC*/    "initialized at all). You can easily use the pygame.init()\n"
    /*DOC*/    "function to cleanly initialize everything, but first use the\n"
    /*DOC*/    "pygame.mixer.pre_init() function to change the default values for\n"
    /*DOC*/    "this init().\n"
    /*DOC*/ ;

static PyObject* init(PyObject* self, PyObject* arg)
{
	PyObject* result;
	int value;

	result = autoinit(self, arg);
	value = PyObject_IsTrue(result);
	Py_DECREF(result);
	if(!value)
		return RAISE(PyExc_SDLError, SDL_GetError());

	RETURN_NONE
}


    /*DOC*/ static char doc_get_init[] =
    /*DOC*/    "pygame.mixer.get_init() -> bool\n"
    /*DOC*/    "query initialization for the mixer\n"
    /*DOC*/    "\n"
    /*DOC*/    "Returns true if the mixer module is initialized.\n"
    /*DOC*/ ;

static PyObject* get_init(PyObject* self, PyObject* arg)
{
	if(!PyArg_ParseTuple(arg, ""))
		return NULL;

	return PyInt_FromLong(SDL_WasInit(SDL_INIT_AUDIO));
}



    /*DOC*/ static char doc_pre_init[] =
    /*DOC*/    "pygame.mixer.pre_init([freq, [size, [stereo]]]) -> None\n"
    /*DOC*/    "presets the init default values\n"
    /*DOC*/    "\n"
    /*DOC*/    "This routine is usefull when you want to customize the sound\n"
    /*DOC*/    "mixer playback modes. The values you pass will change the default\n"
    /*DOC*/    "values used by pygame.mixer.init(). This way you can still use\n"
    /*DOC*/    "the pygame automatic initialization to ensure everything happens\n"
    /*DOC*/    "in the right order, but set the desired mixer mode.\n"
    /*DOC*/ ;

static PyObject* pre_init(PyObject* self, PyObject* arg)
{
	request_frequency = MIX_DEFAULT_FREQUENCY;
	request_size = MIX_DEFAULT_FORMAT;
	request_stereo = request_stereo;

	if(!PyArg_ParseTuple(arg, "|iii", &request_frequency, &request_size,
				&request_stereo))
		return NULL;

	RETURN_NONE
}




/* sound object methods */

    /*DOC*/ static char doc_snd_play[] =
    /*DOC*/    "Sound.play([loops, [maxtime]]) -> Channel\n"
    /*DOC*/    "play sound\n"
    /*DOC*/    "\n"
    /*DOC*/    "Starts playing a song on an available channel. If no channels are\n"
    /*DOC*/    "available, it will not play and return None. Loops controls how\n"
    /*DOC*/    "many extra times the sound will play, a negative loop will play\n"
    /*DOC*/    "indefinitely, it defaults to 0. Maxtime is the number of total\n"
    /*DOC*/    "milliseconds that the sound will play. It defaults to forever\n"
    /*DOC*/    "(-1).\n"
    /*DOC*/    "\n"
    /*DOC*/    "Returns a channel object for the channel that is selected to play\n"
    /*DOC*/    "the sound.\n"
    /*DOC*/ ;

static PyObject* snd_play(PyObject* self, PyObject* args)
{
	Mix_Chunk* chunk = PySound_AsChunk(self);
	int channelnum = -1;
	int loops = 0, playtime = -1;
	
	if(!PyArg_ParseTuple(args, "|ii", &loops, &playtime))
		return NULL;
	
	channelnum = Mix_PlayChannelTimed(-1, chunk, loops, playtime);
	if(channelnum == -1)
		RETURN_NONE

	//make sure volume on this arbitrary channel is set to full
	Mix_Volume(channelnum, 128);

	Mix_GroupChannel(channelnum, (int)chunk);
	return PyChannel_New(channelnum);
}




    /*DOC*/ static char doc_snd_get_num_channels[] =
    /*DOC*/    "Sound.get_num_channels() -> int\n"
    /*DOC*/    "number of channels with sound\n"
    /*DOC*/    "\n"
    /*DOC*/    "Returns the number of channels that have been using this sound.\n"
    /*DOC*/    "The channels may have already finished, but have not started\n"
    /*DOC*/    "playing any other sounds.\n"
    /*DOC*/ ;

static PyObject* snd_get_num_channels(PyObject* self, PyObject* args)
{
	Mix_Chunk* chunk = PySound_AsChunk(self);
	if(!PyArg_ParseTuple(args, ""))
		return NULL;

	MIXER_INIT_CHECK();

	return PyInt_FromLong(Mix_GroupCount((int)chunk));
}


#if 0
static char docXX_snd_get_busy[] =
	"Sound.get_busy() -> int\n"
	"Returns the number of channels this sound is actively playing on";

static PyObject* snd_get_busy(PyObject* self, PyObject* args)
{
	Mix_Chunk* chunk = PySound_AsChunk(self);
	if(!PyArg_ParseTuple(args, ""))
		return NULL;

	MIXER_INIT_CHECK();

	/*there is no call for this in sdl_mixer, yargh */
	return PyInt_FromLong(Mix_PlayingGroup((int)chunk));
}
#endif


#if 0
static char docXX_snd_get_channel[] =
	"Sound.get_channel(int) -> Channel\n"
	"Retrieves a channel index for this sound";

static PyObject* snd_get_channel(PyObject* self, PyObject* args)
{
	Mix_Chunk* chunk = PySound_AsChunk(self);
	int chan;
	if(!PyArg_ParseTuple(args, "i", &chan))
		return NULL;

	MIXER_INIT_CHECK();

	return PyChannel_New(chan);
}
#endif




    /*DOC*/ static char doc_snd_fadeout[] =
    /*DOC*/    "Sound.fadeout(millisec) -> None\n"
    /*DOC*/    "fadeout all channels playing this sound\n"
    /*DOC*/    "\n"
    /*DOC*/    "Fade out all the playing channels playing this sound over the.\n"
    /*DOC*/    "All channels playing this sound will be stopped after the given\n"
    /*DOC*/    "milliseconds.\n"
    /*DOC*/ ;

static PyObject* snd_fadeout(PyObject* self, PyObject* args)
{
	Mix_Chunk* chunk = PySound_AsChunk(self);
	int time;
	if(!PyArg_ParseTuple(args, "i", &time))
		return NULL;

	MIXER_INIT_CHECK();

	Mix_FadeOutGroup((int)chunk, time);
	RETURN_NONE
}


    /*DOC*/ static char doc_snd_stop[] =
    /*DOC*/    "Sound.stop() -> None\n"
    /*DOC*/    "stop all channels playing this sound\n"
    /*DOC*/    "\n"
    /*DOC*/    "This will instantly stop all channels playing this sound.\n"
    /*DOC*/ ;

static PyObject* snd_stop(PyObject* self, PyObject* args)
{
	Mix_Chunk* chunk = PySound_AsChunk(self);
	if(!PyArg_ParseTuple(args, ""))
		return NULL;

	MIXER_INIT_CHECK();

	Mix_HaltGroup((int)chunk);
	RETURN_NONE
}


    /*DOC*/ static char doc_snd_set_volume[] =
    /*DOC*/    "Sound.set_volume(val) -> None\n"
    /*DOC*/    "change volume for sound\n"
    /*DOC*/    "\n"
    /*DOC*/    "Set the play volume for this sound. This will effect any channels\n"
    /*DOC*/    "currently playing this sound, along with all subsequent calls to\n"
    /*DOC*/    "play. The value is 0.0 to 1.0.\n"
    /*DOC*/ ;

static PyObject* snd_set_volume(PyObject* self, PyObject* args)
{
	Mix_Chunk* chunk = PySound_AsChunk(self);
	float volume;

	if(!PyArg_ParseTuple(args, "f", &volume))
		return NULL;

	MIXER_INIT_CHECK();

	Mix_VolumeChunk(chunk, (int)(volume*128));
	RETURN_NONE
}


    /*DOC*/ static char doc_snd_get_volume[] =
    /*DOC*/    "Sound.get_volume() -> val\n"
    /*DOC*/    "query volume for sound\n"
    /*DOC*/    "\n"
    /*DOC*/    "Returns the current volume for this sound object. The value is\n"
    /*DOC*/    "0.0 to 1.0.\n"
    /*DOC*/ ;

static PyObject* snd_get_volume(PyObject* self, PyObject* args)
{
	Mix_Chunk* chunk = PySound_AsChunk(self);
	int volume;

	if(!PyArg_ParseTuple(args, ""))
		return NULL;

	MIXER_INIT_CHECK();

	volume = Mix_VolumeChunk(chunk, -1);
	return PyFloat_FromDouble(volume / 128.0);
}




static PyMethodDef sound_builtins[] =
{
	{ "play", snd_play, 1, doc_snd_play },
	{ "get_num_channels", snd_get_num_channels, 1, doc_snd_get_num_channels },

/*	{ "get_busy", snd_get_busy, doc_snd_get_busy }, */
/*	{ "get_channel", snd_get_channel doc_snd_get_channel }, */
	{ "fadeout", snd_fadeout, 1, doc_snd_fadeout },
	{ "stop", snd_stop, 1, doc_snd_stop },
	{ "set_volume", snd_set_volume, 1, doc_snd_set_volume },
	{ "get_volume", snd_get_volume, 1, doc_snd_get_volume },

	{ NULL, NULL }
};


/*sound object internals*/

static void sound_dealloc(PyObject* self)
{
	Mix_Chunk* chunk = PySound_AsChunk(self);
	Mix_FreeChunk(chunk);
	PyMem_DEL(self);
}


static PyObject* sound_getattr(PyObject* self, char* attrname)
{
	return Py_FindMethod(sound_builtins, self, attrname);
}


    /*DOC*/ static char doc_Sound_MODULE[] =
    /*DOC*/    "Sound objects represent actual sound data. Sound objects are\n"
    /*DOC*/    "created from the function pygame.mixer.load(). Sound objects can\n"
    /*DOC*/    "be playing on multiple channels simultaneously. Calling functions\n"
    /*DOC*/    "like Sound.stop() from the sound objects will effect all channels\n"
    /*DOC*/    "playing that Sound object.\n"
    /*DOC*/    "\n"
    /*DOC*/    "All sound objects have the same frequency and format as the\n"
    /*DOC*/    "pygame.mixer module's initialization.\n"
    /*DOC*/ ;

static PyTypeObject PySound_Type = 
{
	PyObject_HEAD_INIT(NULL)
	0,
	"Sound",
	sizeof(PySoundObject),
	0,
	sound_dealloc,	
	0,
	sound_getattr,
	NULL,					/*setattr*/
	NULL,					/*compare*/
	NULL,					/*repr*/
	NULL,					/*as_number*/
	NULL,					/*as_sequence*/
	NULL,					/*as_mapping*/
	(hashfunc)NULL,			/*hash*/
	(ternaryfunc)NULL,		/*call*/
	(reprfunc)NULL,			/*str*/
	0L,0L,0L,0L,
	doc_Sound_MODULE /* Documentation string */
};



/* channel object methods */


    /*DOC*/ static char doc_chan_play[] =
    /*DOC*/    "Channel.play(Sound, [loops, [maxtime]]) -> None\n"
    /*DOC*/    "play a sound on this channel\n"
    /*DOC*/    "\n"
    /*DOC*/    "Starts playing a given sound on this channel. If the channels is\n"
    /*DOC*/    "currently playing a different sound, it will be\n"
    /*DOC*/    "replaced/restarted with the given sound. Loops controls how many\n"
    /*DOC*/    "extra times the sound will play, a negative loop will play\n"
    /*DOC*/    "indefinitely, it defaults to 0. Maxtime is the number of\n"
    /*DOC*/    "totalmilliseconds that the sound will play. It defaults to\n"
    /*DOC*/    "forever (-1).\n"
    /*DOC*/ ;

static PyObject* chan_play(PyObject* self, PyObject* args)
{
	int channelnum = PyChannel_AsInt(self);
	PyObject* sound;
	Mix_Chunk* chunk;
	int loops = 0, playtime = -1;
	
	if(!PyArg_ParseTuple(args, "O!|ii", &PySound_Type, &sound, &loops, &playtime))
		return NULL;
	chunk = PySound_AsChunk(sound);
	
	channelnum = Mix_PlayChannelTimed(channelnum, chunk, loops, playtime);
	if(channelnum != -1)
		Mix_GroupChannel(channelnum, (int)chunk);
	
	RETURN_NONE
}




    /*DOC*/ static char doc_chan_get_busy[] =
    /*DOC*/    "Channel.get_busy() -> bool\n"
    /*DOC*/    "query state of the channel\n"
    /*DOC*/    "\n"
    /*DOC*/    "Returns true when there is a sound actively playing on this\n"
    /*DOC*/    "channel.\n"
    /*DOC*/ ;

static PyObject* chan_get_busy(PyObject* self, PyObject* args)
{
	int channelnum = PyChannel_AsInt(self);
	if(!PyArg_ParseTuple(args, ""))
		return NULL;

	MIXER_INIT_CHECK();

	return PyInt_FromLong(Mix_Playing(channelnum));
}



    /*DOC*/ static char doc_chan_fadeout[] =
    /*DOC*/    "Channel.fadeout(millisec) -> None\n"
    /*DOC*/    "fade out the channel\n"
    /*DOC*/    "\n"
    /*DOC*/    "Fade out the playing sound and stops it over the\n"
    /*DOC*/    "given millisonds.\n"
    /*DOC*/ ;

static PyObject* chan_fadeout(PyObject* self, PyObject* args)
{
	int channelnum = PyChannel_AsInt(self);
	int time;
	if(!PyArg_ParseTuple(args, "i", &time))
		return NULL;

	MIXER_INIT_CHECK();

	Mix_FadeOutChannel(channelnum, time);
	RETURN_NONE
}


    /*DOC*/ static char doc_chan_stop[] =
    /*DOC*/    "Channel.stop() -> None\n"
    /*DOC*/    "stop playing on the channel\n"
    /*DOC*/    "\n"
    /*DOC*/    "Stops the sound that is playing on this channel.\n"
    /*DOC*/ ;

static PyObject* chan_stop(PyObject* self, PyObject* args)
{
	int channelnum = PyChannel_AsInt(self);
	if(!PyArg_ParseTuple(args, ""))
		return NULL;

	MIXER_INIT_CHECK();

	Mix_HaltChannel(channelnum);
	RETURN_NONE
}


    /*DOC*/ static char doc_chan_pause[] =
    /*DOC*/    "Channel.pause() -> None\n"
    /*DOC*/    "temporarily stop the channel\n"
    /*DOC*/    "\n"
    /*DOC*/    "Stops the sound that is playing on this channel,\n"
    /*DOC*/    "but it can be resumed with a call to unpause()\n"
    /*DOC*/ ;

static PyObject* chan_pause(PyObject* self, PyObject* args)
{
	int channelnum = PyChannel_AsInt(self);
	if(!PyArg_ParseTuple(args, ""))
		return NULL;

	MIXER_INIT_CHECK();

	Mix_Pause(channelnum);
	RETURN_NONE
}


    /*DOC*/ static char doc_chan_unpause[] =
    /*DOC*/    "Channel.unpause() -> None\n"
    /*DOC*/    "restart a paused channel\n"
    /*DOC*/    "\n"
    /*DOC*/    "Restarts a paused channel where it was paused.\n"
    /*DOC*/ ;

static PyObject* chan_unpause(PyObject* self, PyObject* args)
{
	int channelnum = PyChannel_AsInt(self);
	if(!PyArg_ParseTuple(args, ""))
		return NULL;

	MIXER_INIT_CHECK();

	Mix_Resume(channelnum);
	RETURN_NONE
}


    /*DOC*/ static char doc_chan_set_volume[] =
    /*DOC*/    "Channel.set_volume(val) -> None\n"
    /*DOC*/    "set volume for channel\n"
    /*DOC*/    "\n"
    /*DOC*/    "Sets the volume for the channel. The channel's volume level is\n"
    /*DOC*/    "mixed with the volume for the active sound object. The value is\n"
    /*DOC*/    "between 0.0 and 1.0.\n"
    /*DOC*/ ;

static PyObject* chan_set_volume(PyObject* self, PyObject* args)
{
	int channelnum = PyChannel_AsInt(self);
	float volume;

	if(!PyArg_ParseTuple(args, "f", &volume))
		return NULL;

	MIXER_INIT_CHECK();

	Mix_Volume(channelnum, (int)(volume*128));
	RETURN_NONE
}


    /*DOC*/ static char doc_chan_get_volume[] =
    /*DOC*/    "Channel.get_volume() -> val\n"
    /*DOC*/    "query the volume for the\n"
    /*DOC*/    "\n"
    /*DOC*/    "Returns the current volume for this sound object. The value is\n"
    /*DOC*/    "between 0.0 and 1.0.\n"
    /*DOC*/ ;

static PyObject* chan_get_volume(PyObject* self, PyObject* args)
{
	int channelnum = PyChannel_AsInt(self);
	int volume;

	if(!PyArg_ParseTuple(args, ""))
		return NULL;

	MIXER_INIT_CHECK();

	volume = Mix_Volume(channelnum, -1);
	return PyFloat_FromDouble(volume / 128.0);
}





static PyMethodDef channel_builtins[] =
{
	{ "play", chan_play, 1, doc_chan_play },
	{ "get_busy", chan_get_busy, 1, doc_chan_get_busy },
	{ "fadeout", chan_fadeout, 1, doc_chan_fadeout },
	{ "stop", chan_stop, 1, doc_chan_stop },
	{ "pause", chan_pause, 1, doc_chan_pause },
	{ "unpause", chan_unpause, 1, doc_chan_unpause },
	{ "set_volume", chan_set_volume, 1, doc_chan_set_volume },
	{ "get_volume", chan_get_volume, 1, doc_chan_get_volume },

	{ NULL, NULL }
};


/* channel object internals */

static void channel_dealloc(PyObject* self)
{
	PyMem_DEL(self);
}


static PyObject* channel_getattr(PyObject* self, char* attrname)
{
	return Py_FindMethod(channel_builtins, self, attrname);
}


    /*DOC*/ static char doc_Channel_MODULE[] =
    /*DOC*/    "Channel objects represent a single channel of sound. Each channel\n"
    /*DOC*/    "can only playback one Sound object at a time. If your application\n"
    /*DOC*/    "only requires simply sound playback, you will usually not need to\n"
    /*DOC*/    "bother with the Channel objects, they exist for finer playback\n"
    /*DOC*/    "control.\n"
    /*DOC*/    "\n"
    /*DOC*/    "Sound objects can be retrieved from the pygame.mixer module with\n"
    /*DOC*/    "functions like pygame.mixer.Channel() and\n"
    /*DOC*/    "pygame.mixer.find_channel(). Also, each time you call\n"
    /*DOC*/    "Sound.play() a Channel object will be returned, representing the\n"
    /*DOC*/    "channel that sound is playing on.\n"
    /*DOC*/ ;


static PyTypeObject PyChannel_Type = 
{
	PyObject_HEAD_INIT(NULL)
	0,
	"Channel",
	sizeof(PyChannelObject),
	0,
	channel_dealloc,	
	0,
	channel_getattr,
	NULL,					/*setattr*/
	NULL,					/*compare*/
	NULL,					/*repr*/
	NULL,					/*as_number*/
	NULL,					/*as_sequence*/
	NULL,					/*as_mapping*/
	(hashfunc)NULL,			/*hash*/
	(ternaryfunc)NULL,		/*call*/
	(reprfunc)NULL,			/*str*/
	0L,0L,0L,0L,
	doc_Channel_MODULE /* Documentation string */
};



/*mixer module methods*/

    /*DOC*/ static char doc_get_num_channels[] =
    /*DOC*/    "pygame.mixer.get_num_channels() -> int\n"
    /*DOC*/    "query the number of channels\n"
    /*DOC*/    "\n"
    /*DOC*/    "Gets the current number of channels available for the mixer. This\n"
    /*DOC*/    "value can be changed with set_num_channels(). This value defaults\n"
    /*DOC*/    "to 8 when the mixer is first initialized.\n"
    /*DOC*/ ;

static PyObject* get_num_channels(PyObject* self, PyObject* args)
{
	if(!PyArg_ParseTuple(args, ""))
		return NULL;

	MIXER_INIT_CHECK();

	return PyInt_FromLong(Mix_GroupCount(-1));
}


    /*DOC*/ static char doc_set_num_channels[] =
    /*DOC*/    "pygame.mixer.set_num_channels(int) -> None\n"
    /*DOC*/    "sets the number of available channels\n"
    /*DOC*/    "\n"
    /*DOC*/    "Sets the current number of channels available for the mixer. This\n"
    /*DOC*/    "value defaults to 8 when the mixer is first initialized. If the\n"
    /*DOC*/    "value is decreased, sounds playing in channels above the new\n"
    /*DOC*/    "value will stop.\n"
    /*DOC*/ ;

static PyObject* set_num_channels(PyObject* self, PyObject* args)
{
	int numchans;
	if(!PyArg_ParseTuple(args, "i", &numchans))
		return NULL;

	MIXER_INIT_CHECK();

	Mix_AllocateChannels(numchans);
	RETURN_NONE
}


    /*DOC*/ static char doc_set_reserved[] =
    /*DOC*/    "pygame.mixer.set_reserved(int) -> None\n"
    /*DOC*/    "reserves first given channels\n"
    /*DOC*/    "\n"
    /*DOC*/    "Reserves the first channels. Reserved channels won't be used when\n"
    /*DOC*/    "a sound is played without using a specific channel object.\n"
    /*DOC*/ ;

static PyObject* set_reserved(PyObject* self, PyObject* args)
{
	int numchans;
	if(!PyArg_ParseTuple(args, "i", &numchans))
		return NULL;

	MIXER_INIT_CHECK();

	Mix_ReserveChannels(numchans);
	RETURN_NONE
}


    /*DOC*/ static char doc_get_busy[] =
    /*DOC*/    "pygame.mixer.get_busy() -> int\n"
    /*DOC*/    "query busy channels\n"
    /*DOC*/    "\n"
    /*DOC*/    "Returns the number of current active channels. This is not the\n"
    /*DOC*/    "total channels, but the number of channels that are currently\n"
    /*DOC*/    "playing sound.\n"
    /*DOC*/ ;

static PyObject* get_busy(PyObject* self, PyObject* args)
{
	if(!PyArg_ParseTuple(args, ""))
		return NULL;

	if(!SDL_WasInit(SDL_INIT_AUDIO))
		return PyInt_FromLong(0);

	return PyInt_FromLong(Mix_Playing(-1));
}


    /*DOC*/ static char doc_Channel[] =
    /*DOC*/    "pygame.mixer.Channel(int) -> Channel\n"
    /*DOC*/    "get channel object\n"
    /*DOC*/    "\n"
    /*DOC*/    "Get a channel object for the given channel. This number must be\n"
    /*DOC*/    "less that the current number of channels.\n"
    /*DOC*/ ;

static PyObject* Channel(PyObject* self, PyObject* args)
{
	int chan;
	if(!PyArg_ParseTuple(args, "i", &chan))
		return NULL;

	MIXER_INIT_CHECK();

	return PyChannel_New(chan);
}



    /*DOC*/ static char doc_find_channel[] =
    /*DOC*/    "pygame.mixer.find_channel([force]) -> Channel\n"
    /*DOC*/    "find an available sound channel\n"
    /*DOC*/    "\n"
    /*DOC*/    "Find a sound channel that is not busy. If the force argument is\n"
    /*DOC*/    "passed as a nonzero number, this will return the channel of the\n"
    /*DOC*/    "longest running sound. If not forced, and there are no available\n"
    /*DOC*/    "channels, returns None.\n"
    /*DOC*/ ;

static PyObject* find_channel(PyObject* self, PyObject* args)
{
	int chan, force = 0;
	if(!PyArg_ParseTuple(args, "|i", &force))
		return NULL;

	MIXER_INIT_CHECK();

	chan = Mix_GroupAvailable(-1);
	if(chan == -1)
	{
		if(!force)
			RETURN_NONE
		chan = Mix_GroupOldest(-1);
	}
	return PyChannel_New(chan);
}


    /*DOC*/ static char doc_fadeout[] =
    /*DOC*/    "pygame.mixer.fadeout(millisec) -> None\n"
    /*DOC*/    "fadeout all channels\n"
    /*DOC*/    "\n"
    /*DOC*/    "Fade out all the playing channels over the given number of\n"
    /*DOC*/    "milliseconds.\n"
    /*DOC*/ ;

static PyObject* fadeout(PyObject* self, PyObject* args)
{
	int time;
	if(!PyArg_ParseTuple(args, "i", &time))
		return NULL;

	MIXER_INIT_CHECK();

	Mix_FadeOutChannel(-1, time);
	RETURN_NONE
}


    /*DOC*/ static char doc_stop[] =
    /*DOC*/    "pygame.mixer.stop() -> None\n"
    /*DOC*/    "stop all channels\n"
    /*DOC*/    "\n"
    /*DOC*/    "Stop the playback on all mixer channels.\n"
    /*DOC*/ ;

static PyObject* stop(PyObject* self, PyObject* args)
{
	if(!PyArg_ParseTuple(args, ""))
		return NULL;

	MIXER_INIT_CHECK();

	Mix_HaltChannel(-1);
	RETURN_NONE
}


    /*DOC*/ static char doc_pause[] =
    /*DOC*/    "pygame.mixer.pause() -> None\n"
    /*DOC*/    "pause all channels\n"
    /*DOC*/    "\n"
    /*DOC*/    "Temporarily stops playback on all the mixer channels.\n"
    /*DOC*/ ;

static PyObject* pause(PyObject* self, PyObject* args)
{
	if(!PyArg_ParseTuple(args, ""))
		return NULL;

	MIXER_INIT_CHECK();

	Mix_Pause(-1);
	RETURN_NONE
}


    /*DOC*/ static char doc_unpause[] =
    /*DOC*/    "pygame.mixer.unpause() -> None\n"
    /*DOC*/    "restart any pause channels\n"
    /*DOC*/    "\n"
    /*DOC*/    "Restarts playback of any paused channels.\n"
    /*DOC*/ ;

static PyObject* unpause(PyObject* self, PyObject* args)
{
	if(!PyArg_ParseTuple(args, ""))
		return NULL;

	MIXER_INIT_CHECK();

	Mix_Resume(-1);
	RETURN_NONE
}



    /*DOC*/ static char doc_Sound[] =
    /*DOC*/    "pygame.mixer.Sound(file) -> Sound\n"
    /*DOC*/    "load a new soundfile\n"
    /*DOC*/    "\n"
    /*DOC*/    "Loads a new sound object from a WAV file. File can be a filename\n"
    /*DOC*/    "or a file-like object. The sound will be converted to match the\n"
    /*DOC*/    "current mode of the mixer.\n"
    /*DOC*/ ;

static PyObject* Sound(PyObject* self, PyObject* arg)
{
	PyObject* file;
	char* name = NULL;
	Mix_Chunk* chunk;
	SDL_RWops *rw;
	if(!PyArg_ParseTuple(arg, "O", &file))
		return NULL;

	MIXER_INIT_CHECK();

	if(PyString_Check(file))
	{
		name = PyString_AsString(file);
		Py_BEGIN_ALLOW_THREADS
		chunk = Mix_LoadWAV(name);
		Py_END_ALLOW_THREADS
	}
	else
	{
		if(!(rw = RWopsFromPython(file)))
			return NULL;
		Py_BEGIN_ALLOW_THREADS
		chunk = Mix_LoadWAV_RW(rw, 1);
		Py_END_ALLOW_THREADS
	}

	if(!chunk)
		return RAISE(PyExc_SDLError, SDL_GetError());

	return PySound_New(chunk);
}




static PyMethodDef mixer_builtins[] =
{
	{ "__PYGAMEinit__", autoinit, 1, doc_init },
	{ "init", init, 1, doc_init },
	{ "quit", quit, 1, doc_quit },
	{ "get_init", get_init, 1, doc_get_init },
	{ "pre_init", pre_init, 1, doc_pre_init },

	{ "get_num_channels", get_num_channels, 1, doc_get_num_channels },
	{ "set_num_channels", set_num_channels, 1, doc_set_num_channels },
	{ "set_reserved", set_reserved, 1, doc_set_reserved },

	{ "get_busy", get_busy, 1, doc_get_busy },
	{ "Channel", Channel, 1, doc_Channel },
	{ "find_channel", find_channel, 1, doc_find_channel },
	{ "fadeout", fadeout, 1, doc_fadeout },
	{ "stop", stop, 1, doc_stop },
	{ "pause", pause, 1, doc_pause },
	{ "unpause", unpause, 1, doc_unpause },
/*	{ "lookup_frequency", lookup_frequency, 1, doc_lookup_frequency },*/

	{ "Sound", Sound, 1, doc_Sound },

	{ NULL, NULL }
};



static PyObject* PySound_New(Mix_Chunk* chunk)
{
	PySoundObject* soundobj;
	
	if(!chunk)
		return RAISE(PyExc_RuntimeError, "unable to create sound.");

	soundobj = PyObject_NEW(PySoundObject, &PySound_Type);
	if(soundobj)
	{
		soundobj->chunk = chunk;
	}
	else
	{
		/*free the sound here, nobody else can*/
		Mix_FreeChunk(chunk);
	}

	return (PyObject*)soundobj;
}



static PyObject* PyChannel_New(int channelnum)
{
	PyChannelObject* chanobj;
	
	if(channelnum < 0 || channelnum >= Mix_GroupCount(-1))
		return RAISE(PyExc_IndexError, "invalid channel index");

	chanobj = PyObject_NEW(PyChannelObject, &PyChannel_Type);
	if(!chanobj)
		return NULL;

	chanobj->chan = channelnum;
	return (PyObject*)chanobj;
}



    /*DOC*/ static char doc_pygame_mixer_MODULE[] =
    /*DOC*/    "Contains sound mixer routines and objects. The mixer module is an\n"
    /*DOC*/    "optional pygame module, dependent on the SDL_mixer library. This\n"
    /*DOC*/    "module contains the usual routines needed to initialize the\n"
    /*DOC*/    "module. One difference is the pygame.mixer.init() function takes\n"
    /*DOC*/    "several optional arguments. These arguments control the playback\n"
    /*DOC*/    "rates and datatypes for the sound playback. If you do need\n"
    /*DOC*/    "specific control over the playback rate, but don't want to bother\n"
    /*DOC*/    "with hand-initializing the modules, there is a function named\n"
    /*DOC*/    "pygame.mixer.pre_init() which takes the same arguments as init(),\n"
    /*DOC*/    "but only sets the new default values. You can call this before\n"
    /*DOC*/    "pygame.init() and not have to worry about the pygame module\n"
    /*DOC*/    "initialization order.\n"
    /*DOC*/    "\n"
    /*DOC*/    "Sound objects are created from the pygame.mixer.load() function.\n"
    /*DOC*/    "Simple sound playback can simply use the Sound.play() method to\n"
    /*DOC*/    "play the sound. Each Sound object can be played multiple times\n"
    /*DOC*/    "simultaneously. If you desire more specific control over the\n"
    /*DOC*/    "Sound objects, you can access the Channel objects with functions\n"
    /*DOC*/    "like pygame.mixer.Channel().\n"
    /*DOC*/    "\n"
    /*DOC*/    "The mixer defaults to supporting 8 simultaneous soundfiles.\n"
    /*DOC*/    "You can change the number of available sound channels at any\n"
    /*DOC*/    "time with the set_num_channels() function.\n"
    /*DOC*/    "\n"
    /*DOC*/    "All loaded Sound objects are resampled to match the same format\n"
    /*DOC*/    "that pygame.mixer is initialized to. The current SDL resampling\n"
    /*DOC*/    "functions are not that good, so it is best if you initialize\n"
    /*DOC*/    "pygame.mixer to the same format as your sound resources. Also\n"
    /*DOC*/    "setting the mixer frequency to even multiples of your sound\n"
    /*DOC*/    "resources will result in a cleaner conversion.\n"
    /*DOC*/    "\n"
    /*DOC*/    "The mixer also contains a special channel for music. You can\n"
    /*DOC*/    "control the music channel through pygame.mixer.music./\n"
    /*DOC*/ ;

PYGAME_EXPORT
void initmixer(void)
{
	PyObject *module, *dict, *apiobj, *music;
	static void* c_api[PYGAMEAPI_MIXER_NUMSLOTS];

	PyMIXER_C_API[0] = PyMIXER_C_API[0]; /*this cleans an unused warning*/

	PyType_Init(PySound_Type);
	PyType_Init(PyChannel_Type);

    /* create the module */
	module = Py_InitModule3("mixer", mixer_builtins, doc_pygame_mixer_MODULE);
	dict = PyModule_GetDict(module);

	PyDict_SetItemString(dict, "SoundType", (PyObject *)&PySound_Type);
	PyDict_SetItemString(dict, "ChannelType", (PyObject *)&PyChannel_Type);

	/* export the c api */
	c_api[0] = &PySound_Type;
	c_api[1] = PySound_New;
	c_api[2] = snd_play;
	c_api[3] = &PyChannel_Type;
	c_api[4] = PyChannel_New;
	c_api[5] = autoinit;
	c_api[6] = autoquit;
	apiobj = PyCObject_FromVoidPtr(c_api, NULL);
	PyDict_SetItemString(dict, PYGAMEAPI_LOCAL_ENTRY, apiobj);

	/*imported needed apis*/
	import_pygame_base();

	music = PyImport_ImportModule("pygame.mixer_music");
	if(music) 
		PyModule_AddObject(module, "music", music);
	else /*music module not compiled? cleanly ignore*/
		PyErr_Clear();
}

