class AppSettings;

/**
 * Frame splitter positions
 */
var int Splitter1;
var int Splitter2;


/**
 * Window positions
 */
struct WndPos
{
	var int  x, y, w, h;
	var bool maximized;
};

var WndPos MainFramePos;
var WndPos LogFramePos;
var WndPos SettingsFramePos;

/**
 * PropEditor's splitter positions
 */
var int MeshPropSplitter;
var int AnimSetPropSplitter;
var int AnimSeqPropSplitter;
var int PrefsPropSplitter;

/**
 * #DIRNAME
 * Base directory with game resources.
 * Should be set for correct editor functionality.
 * Changes in this value will require to reload mesh.
 */
var(Paths) string[256] ResourceRoot;
/**
 * #DIRNAME
 * Directory, containing exported from modeling program PSK and PSA files.
 */
var(Paths) string[256] ImportDirectory;
/**
 * #DIRNAME
 * Directory, containing engine animation resources.
 * Used by 'open mesh' etc dialog.
 */
var(Paths) string[256] AnimDataDirectory;
/**
 * Background colour of mesh viewer/editor
 */
var(Colors) Color3f MeshBackground;
