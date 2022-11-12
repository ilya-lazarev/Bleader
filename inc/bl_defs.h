#pragma once
#ifndef _BL_DEFS_H_
#define _BL_DEFS_H_

/* INTEGER CODES */
#ifdef __BIG_ENDIAN__
/* Big Endian */
#  define BLEND_MAKE_ID(a, b, c, d) ((int)(a) << 24 | (int)(b) << 16 | (c) << 8 | (d))
#else
/* Little Endian */
#  define BLEND_MAKE_ID(a, b, c, d) ((int)(d) << 24 | (int)(c) << 16 | (b) << 8 | (a))
#endif

/**
 * Codes used for #BHead.code.
 *
 * These coexist with ID codes such as #ID_OB, #ID_SCE ... etc.
 */
enum {
	/**
	 * Arbitrary allocated memory
	 * (typically owned by #ID's, will be freed when there are no users).
	 */
	DATA = BLEND_MAKE_ID('D', 'A', 'T', 'A'),
	/**
	 * Used for #Global struct.
	 */
	 GLOB = BLEND_MAKE_ID('G', 'L', 'O', 'B'),
	 /**
	  * Used for storing the encoded SDNA string
	  * (decoded into an #SDNA on load).
	  */
	  DNA1 = BLEND_MAKE_ID('D', 'N', 'A', '1'),
	  /**
	   * Used to store thumbnail previews, written between #REND and #GLOB blocks,
	   * (ignored for regular file reading).
	   */
	   TEST = BLEND_MAKE_ID('T', 'E', 'S', 'T'),
	   /**
		* Used for #RenderInfo, basic Scene and frame range info,
		* can be easily read by other applications without writing a full blend file parser.
		*/
		REND = BLEND_MAKE_ID('R', 'E', 'N', 'D'),
		/**
		 * Used for #UserDef, (user-preferences data).
		 * (written to #BLENDER_STARTUP_FILE & #BLENDER_USERPREF_FILE).
		 */
		 USER = BLEND_MAKE_ID('U', 'S', 'E', 'R'),
		 /**
		  * Terminate reading (no data).
		  */
		  ENDB = BLEND_MAKE_ID('E', 'N', 'D', 'B'),
};

typedef enum eObjectMode {
	OB_MODE_OBJECT = 0,
	OB_MODE_EDIT = 1 << 0,
	OB_MODE_SCULPT = 1 << 1,
	OB_MODE_VERTEX_PAINT = 1 << 2,
	OB_MODE_WEIGHT_PAINT = 1 << 3,
	OB_MODE_TEXTURE_PAINT = 1 << 4,
	OB_MODE_PARTICLE_EDIT = 1 << 5,
	OB_MODE_POSE = 1 << 6,
	OB_MODE_EDIT_GPENCIL = 1 << 7,
	OB_MODE_PAINT_GPENCIL = 1 << 8,
	OB_MODE_SCULPT_GPENCIL = 1 << 9,
	OB_MODE_WEIGHT_GPENCIL = 1 << 10,
	OB_MODE_VERTEX_GPENCIL = 1 << 11,
	OB_MODE_SCULPT_CURVES = 1 << 12,
} eObjectMode;

/** #Object.dt, #View3DShading.type */
typedef enum eDrawType {
	OB_BOUNDBOX = 1,
	OB_WIRE = 2,
	OB_SOLID = 3,
	OB_MATERIAL = 4,
	OB_TEXTURE = 5,
	OB_RENDER = 6,
} eDrawType;

/** Any mode where the brush system is used. */
#define OB_MODE_ALL_PAINT \
  (OB_MODE_SCULPT | OB_MODE_VERTEX_PAINT | OB_MODE_WEIGHT_PAINT | OB_MODE_TEXTURE_PAINT)

#define OB_MODE_ALL_PAINT_GPENCIL \
  (OB_MODE_PAINT_GPENCIL | OB_MODE_SCULPT_GPENCIL | OB_MODE_WEIGHT_GPENCIL | \
   OB_MODE_VERTEX_GPENCIL)

/** Any mode that uses Object.sculpt. */
#define OB_MODE_ALL_SCULPT (OB_MODE_SCULPT | OB_MODE_VERTEX_PAINT | OB_MODE_WEIGHT_PAINT)

/** Any mode that uses weightpaint. */
#define OB_MODE_ALL_WEIGHT_PAINT (OB_MODE_WEIGHT_PAINT | OB_MODE_WEIGHT_GPENCIL)

/**
 * Any mode that has data or for Grease Pencil modes, we need to free when switching modes,
 * see: #ED_object_mode_generic_exit
 */
#define OB_MODE_ALL_MODE_DATA \
  (OB_MODE_EDIT | OB_MODE_VERTEX_PAINT | OB_MODE_WEIGHT_PAINT | OB_MODE_SCULPT | OB_MODE_POSE | \
   OB_MODE_PAINT_GPENCIL | OB_MODE_EDIT_GPENCIL | OB_MODE_SCULPT_GPENCIL | \
   OB_MODE_WEIGHT_GPENCIL | OB_MODE_VERTEX_GPENCIL | OB_MODE_SCULPT_CURVES)


enum class eValueType {
	String,
	Int,
	Array,
	Null,
	Boolean,
	Double,
	Dictionary,
};

enum eFileDataFlag {
	FD_FLAGS_SWITCH_ENDIAN = 1 << 0,
	FD_FLAGS_FILE_POINTSIZE_IS_4 = 1 << 1,
	FD_FLAGS_POINTSIZE_DIFFERS = 1 << 2,
	FD_FLAGS_FILE_OK = 1 << 3,
	FD_FLAGS_IS_MEMFILE = 1 << 4,
	/* XXX Unused in practice (checked once but never set). */
	FD_FLAGS_NOT_MY_LIBMAP = 1 << 5,
};

#endif