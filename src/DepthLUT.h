#pragma once
#include "ofMain.h"

// 1D LUT
class DepthLookupTable
{
public:

    DepthLookupTable() :
        mLUTSize( 4096 ),
        mNearMM( 1 ),	// 1mm - to keep 0mm as null value
        mFarMM( 65535 ),
        mbLUTDirty( true )
    {
        generate();
    }

    inline ofTexture& process( const ofTexture& depthInput )
    {
		allocate( depthInput );
        if( mbLUTDirty ) { generate(); }

		if (mFbo.isAllocated()) {
			mFbo.begin();
			ofClear( 0, 0 );



			mFbo.end();
		}
		return mFbo.getTexture();

    }

    inline void setRangeMM( unsigned short nearMM, unsigned short farMM )	{ setNearMM( nearMM ); setFarMM( farMM ); }
    inline void setNearMM( unsigned short nearMM )	{ mNearMM	= nearMM > 1 ? nearMM : 1;		mbLUTDirty = true; }
    inline void setFarMM( unsigned short farMM )	{ mFarMM	= farMM;	mbLUTDirty = true; }


protected:

    ofFbo				mFbo;
    ofPlanePrimitive	mQuad;

    unsigned short		mNearMM, mFarMM;

    GLuint				mLUTTex;
    int					mLUTSize;
    bool				mbLUTDirty;

    string				fragShaderSrc, vertShaderSrc;
    ofShader			fragShader, vertShader;

	// allocate the FBO and resize the quad based on texture input
	inline void allocate(const ofTexture& prototype)
	{
		if (!prototype.isAllocated()) {
			mFbo.clear();
			mQuad = ofPlanePrimitive();
		}
		else if (!mFbo.isAllocated() || mFbo.getWidth() != prototype.getWidth() || mFbo.getHeight() != prototype.getHeight()) {
			float sz[2] = { prototype.getWidth(), prototype.getHeight() };
			mFbo.allocate( sz[0], sz[1], GL_RGB );
			mQuad.set( sz[0], sz[1], 2, 2 );
			mQuad.setPosition( sz[0] * 0.5f, sz[1] * 0.5f, 0.f );	// center in fbo
			mQuad.mapTexCoords( 0, 0, 1, 1 );	// normalized tex coords
		}
	}

    // generate the LUT
    inline void generate()
    {
        // based on 3D LUT here
        // https://github.com/youandhubris/GPU_LUT_in_openFrameworks/blob/master/src/ofApp.cpp

        struct	Rgb {
            Rgb( float v ) : r( v ), g( v ), b( v ) {}
            float r, g, b;
        };

        vector<Rgb> lut;
        lut.reserve( mLUTSize );

        lut.push_back( Rgb( 0. ) );		// 0 mm == no data -> black (0)

        for( int i = 1; i < mLUTSize; ++i ) {
            float inputVal	= ofMap( i, 1, mLUTSize - 1, 0, 65355, true );					// map lookup indices to unsigned short range
            float outputVal = ofMap( inputVal, mFarMM, mNearMM, 0.0000001f, 1.f, true );	// map depth range to luma values (near = white, far = black + epsilon)
            lut.push_back( Rgb( outputVal ) );
        }


        // create 1D texture with LUT data

        glEnable( GL_TEXTURE_1D );

        glGenTextures( 1, &mLUTTex );
        glBindTexture( GL_TEXTURE_1D, mLUTTex );

        glTexParameteri( GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
        glTexParameteri( GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

        glTexParameteri( GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_REPEAT );
        glTexParameteri( GL_TEXTURE_1D, GL_TEXTURE_WRAP_T, GL_REPEAT );

        glTexImage1D( GL_TEXTURE_1D, 0, GL_RGB, mLUTSize, 0, GL_RGB, GL_FLOAT, &lut[0] );

        glBindTexture( GL_TEXTURE_1D, 0 );
        glDisable( GL_TEXTURE_1D );

        mbLUTDirty = false;
    }


    ////////////
    /* SHADER */
    ////////////

	// *INDENT-OFF*
	#define GLSL150(shader)  "#version 150 \n" #shader

	inline void gl3()
	{
		vertShaderSrc = GLSL150
		(
			uniform mat4 modelViewProjectionMatrix;
			uniform mat4 textureMatrix;

			in vec4	position;
			in vec2	texcoord;
			in vec4	color;

			out vec2 texCoordVarying;
			out vec4 colorVarying;

			void main()
			{
				gl_Position		= modelViewProjectionMatrix * position;
				texCoordVarying = (textureMatrix*vec4(texcoord.x, texcoord.y, 0, 1)).xy;
				colorVarying	= color;
			}
		);


		fragShaderSrc = GLSL150
		(

			uniform sampler2D depthTex;
			uniform sampler1D lutTex;
			uniform float lutSize;

			in vec2 texCoordVarying;
			in vec2 colorVarying;

			out vec4 fragColor;

			void main() {

				// Based on "GPU Gems 2 — Chapter 24. Using Lookup Tables to Accelerate Color Transformations"
				// More info and credits @ http://http.developer.nvidia.com/GPUGems2/gpugems2_chapter24.html

				float rawLum	= texture2D(depthTex, texCoordVarying).r;

				// Compute the 1D LUT lookup scale/offset factor
				float scale		= (lutSize - 1.0) / lutSize;
				float offset	= 1.0 / (2.0 * lutSize);

				// ****** Apply 1D LUT color transform! **************
				// This is our dependent texture read; The 1D texture's
				// lookup coordinate is dependent on the
				// previous texture read's result

				vec3 color		= texture1D(lutTex, rawLum * scale + offset).rgb;

				fragColor		= vec4(color, 1.0);

			}
		);
	}

	#undef GLSL150
	// *INDENT-ON*


};