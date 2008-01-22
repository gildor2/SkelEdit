class AppSettings;

var int Splitter1;
var int Splitter2;


struct WndPos
{
	var int  x, y, w, h;
	var bool maximized;
};

var WndPos MainFramePos;
var WndPos LogFramePos;
var WndPos SettingsFramePos;

// splitter positions
var int MeshPropSplitter;
var int AnimSetPropSplitter;
var int AnimSeqPropSplitter;
var int PrefsPropSplitter;

/**
 * #DIRNAME
 * Base directory with game resources
 */
var() string[256] ResourceRoot;
