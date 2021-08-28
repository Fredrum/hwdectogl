///-----------------------------------------------///
///	Example program for setting up a hardware     ///
/// path for camera/video over v4l2 to Open GL    ///
/// texture.						 		  	  ///
///-----------------------------------------------///
//
// sudo apt install libglfw3-dev libgles2-mesa-dev

#define GLFW_INCLUDE_ES2
#include <GLFW/glfw3.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <linux/videodev2.h>
#include <errno.h>
#include <fcntl.h>
#include <drm_fourcc.h>

#include "glhelp.h"

// video file decode
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
#include <libavutil/hwcontext.h>
#include <libavutil/opt.h>
#include <libavutil/avassert.h>
#include <libavutil/imgutils.h>

static AVBufferRef *hw_device_ctx = NULL;
static enum AVPixelFormat hw_pix_fmt;
static FILE *output_file = NULL;

static int hw_decoder_init(AVCodecContext *ctx, const enum AVHWDeviceType type)
{
    int err = 0;

    if ((err = av_hwdevice_ctx_create(&hw_device_ctx, type,
                                      NULL, NULL, 0)) < 0) {
        fprintf(stderr, "Failed to create specified HW device.\n");
        return err;
    }
    ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);

    return err;
}

static enum AVPixelFormat get_hw_format(AVCodecContext *ctx,
                                        const enum AVPixelFormat *pix_fmts)
{
    const enum AVPixelFormat *p;

    for (p = pix_fmts; *p != -1; p++) {
        if (*p == hw_pix_fmt)
            return *p;
    }

    fprintf(stderr, "Failed to get HW surface format.\n");
    return AV_PIX_FMT_NONE;
}

static int hacky_write_packet(AVFrame *src_frame)
{
	//const AVFrame * const src_frame = (AVFrame *)pkt->data;
    AVFrame * frame;
    
    frame = av_frame_alloc();
    
    int size = av_image_get_buffer_size(src_frame->format, src_frame->width,
                                        src_frame->height, 1);
        
    printf("Frame Size: %d\n", size);  // Size: 3110400   

    //av_frame_ref(frame, src_frame);
    av_hwframe_map(frame, src_frame, 0);
    
    av_frame_free(&frame);
    
	usleep(3000);
}



//~ static int egl_vout_write_packet(AVFormatContext *s, AVPacket *pkt)
//~ {
    //~ const AVFrame * const src_frame = (AVFrame *)pkt->data;
    //~ AVFrame * frame;
    //~ //egl_display_env_t * const de = s->priv_data;

//~ #if TRACE_ALL
    //~ av_log(s, AV_LOG_INFO, "%s\n", __func__);
//~ #endif

    //~ if (src_frame->format == AV_PIX_FMT_DRM_PRIME) {
        //~ frame = av_frame_alloc();
        //~ av_frame_ref(frame, src_frame);
    //~ }
    //~ else if (src_frame->format == AV_PIX_FMT_VAAPI) {
        //~ frame = av_frame_alloc();
        //~ frame->format = AV_PIX_FMT_DRM_PRIME;
        //~ if (av_hwframe_map(frame, src_frame, 0) != 0)
        //~ {
            //~ av_log(s, AV_LOG_WARNING, "Failed to map frame (format=%d) to DRM_PRiME\n", src_frame->format);
            //~ av_frame_free(&frame);
            //~ return AVERROR(EINVAL);
        //~ }
    //~ }
    //~ else {
        //~ av_log(s, AV_LOG_WARNING, "Frame (format=%d) not DRM_PRiME\n", src_frame->format);
        //~ return AVERROR(EINVAL);
    //~ }

    //~ // Really hacky sync
    //~ while (de->show_all && de->q_next) {
       //~ usleep(3000);
    //~ }

    //~ pthread_mutex_lock(&de->q_lock);
    //~ {
        //~ AVFrame * const t = de->q_next;
        //~ de->q_next = frame;
        //~ frame = t;
    //~ }
    //~ pthread_mutex_unlock(&de->q_lock);

    //~ if (frame == NULL)
        //~ sem_post(&de->q_sem);
    //~ else
        //~ av_frame_free(&frame);

    //~ return 0;
//~ }

static int decode_write(AVCodecContext *avctx, AVPacket *packet)
{
    AVFrame *frame = NULL, *sw_frame = NULL;
    AVFrame *tmp_frame = NULL;
    uint8_t *buffer = NULL;
    int size;
    int ret = 0;

    ret = avcodec_send_packet(avctx, packet);
    if (ret < 0) {
        fprintf(stderr, "Error during decoding\n");
        return ret;
    }

    while (1) {
        if (!(frame = av_frame_alloc()) || !(sw_frame = av_frame_alloc())) {
            fprintf(stderr, "Can not alloc frame\n");
            ret = AVERROR(ENOMEM);
            goto fail;
        }

        ret = avcodec_receive_frame(avctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            av_frame_free(&frame);
            av_frame_free(&sw_frame);
            return 0;
        } else if (ret < 0) {
            fprintf(stderr, "Error while decoding\n");
            goto fail;
        }

	printf("GOT FRAME\n");
	hacky_write_packet(frame);
	//if(drm_vout_write_packet(&egl_env, frame) != 0) {
	// printf("Error writing packet to screen");
	//}

    fail:
        av_frame_free(&frame);
        av_frame_free(&sw_frame);
        av_freep(&buffer);
        if (ret < 0)
            return ret;
    }
}

//orig
static const GLuint WIDTH = 1280;
static const GLuint HEIGHT = 720;

static const GLchar* vertex_shader_source =
	"#version 300 es\n"
	"in vec3 position;\n"
	"in vec2 tx_coords;\n"
	"out vec2 v_texCoord;\n"
	"void main() {  \n"
	"	gl_Position = vec4(position, 1.0);\n"
	"	v_texCoord = tx_coords;\n"
	"}\n";
	
static const GLchar* fragment_shader_source =
	"#version 300 es\n"
	"#extension GL_OES_EGL_image_external : require\n"
	"precision mediump float;\n"
	"uniform samplerExternalOES texture;\n"
	"in vec2 v_texCoord;\n"
	"out vec4 out_color;\n"
	"void main() {	\n"
	"	out_color = texture2D( texture, v_texCoord );\n"
	"}\n";

/// negative x,y is bottom left and first vertex
static const GLfloat vertices[][4][3] =
{
    { {-1.0, -1.0, 0.0}, { 1.0, -1.0, 0.0}, {-1.0, 1.0, 0.0}, {1.0, 1.0, 0.0} }
};
static const GLfloat uv_coords[][4][2] =
{
    { {0.0, 0.0}, {1.0, 0.0}, {0.0, 1.0}, {1.0, 1.0} }
};

GLint common_get_shader_program(const char *vertex_shader_source, const char *fragment_shader_source) {
	enum Consts {INFOLOG_LEN = 512};
	GLchar infoLog[INFOLOG_LEN];
	GLint fragment_shader;
	GLint shader_program;
	GLint success;
	GLint vertex_shader;

	/* Vertex shader */
	vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertex_shader, 1, &vertex_shader_source, NULL);
	glCompileShader(vertex_shader);
	glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(vertex_shader, INFOLOG_LEN, NULL, infoLog);
		printf("ERROR::SHADER::VERTEX::COMPILATION_FAILED\n%s\n", infoLog);
	}

	/* Fragment shader */
	fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragment_shader, 1, &fragment_shader_source, NULL);
	glCompileShader(fragment_shader);
	glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(fragment_shader, INFOLOG_LEN, NULL, infoLog);
		printf("ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n%s\n", infoLog);
	}

	/* Link shaders */
	shader_program = glCreateProgram();
	glAttachShader(shader_program, vertex_shader);
	glAttachShader(shader_program, fragment_shader);
	glLinkProgram(shader_program);
	glGetProgramiv(shader_program, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(shader_program, INFOLOG_LEN, NULL, infoLog);
		printf("ERROR::SHADER::PROGRAM::LINKING_FAILED\n%s\n", infoLog);
	}

	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);
	return shader_program;
}

int main(int argc, char *argv[]) {

	GLuint shader_program, vbo;
	GLint pos;
	GLint uvs;
	GLFWwindow* window;

	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
	glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
	glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
	window = glfwCreateWindow(WIDTH, HEIGHT, __FILE__, NULL, NULL);
	glfwMakeContextCurrent(window);

	EGLDisplay egl_display = glfwGetEGLDisplay();
	if(egl_display == EGL_NO_DISPLAY) {
		printf("error: glfwGetEGLDisplay no EGLDisplay returned\n");
	}

	printf("GL_VERSION  : %s\n", glGetString(GL_VERSION) );
	printf("GL_RENDERER : %s\n", glGetString(GL_RENDERER) );

	shader_program = common_get_shader_program(vertex_shader_source, fragment_shader_source);
	pos = glGetAttribLocation(shader_program, "position");
	uvs = glGetAttribLocation(shader_program, "tx_coords");

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glViewport(0, 0, WIDTH, HEIGHT);

	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices)+sizeof(uv_coords), 0, GL_STATIC_DRAW);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
		glBufferSubData(GL_ARRAY_BUFFER, sizeof(vertices), sizeof(uv_coords), uv_coords);
	glEnableVertexAttribArray(pos);
	glEnableVertexAttribArray(uvs);
	glVertexAttribPointer(pos, 3, GL_FLOAT, GL_FALSE, 0, (GLvoid*)0);
	glVertexAttribPointer(uvs, 2, GL_FLOAT, GL_FALSE, 0, sizeof(vertices)); /// last is offset to loc in buf memory
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	/// END Open GL setup ------------------------------------
	/// BEGIN V4L2 setup -------------------------------------
	
	AVFormatContext *input_ctx = NULL;
    int video_stream, ret;
    AVStream *video = NULL;
    AVCodecContext *decoder_ctx = NULL;
    AVCodec *softwareDecoder = NULL;
    AVCodec *decoder = NULL;
    AVPacket packet;
    enum AVHWDeviceType type;
    int i;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <input file> \n", argv[0]);
        return -1;
    }

    type = av_hwdevice_find_type_by_name("drm");
    if (type == AV_HWDEVICE_TYPE_NONE) {
        fprintf(stderr, "Device type drm is not supported.\n");
        fprintf(stderr, "Available device types:");
        while((type = av_hwdevice_iterate_types(type)) != AV_HWDEVICE_TYPE_NONE)
            fprintf(stderr, " %s", av_hwdevice_get_type_name(type));
        fprintf(stderr, "\n");
        return -1;
    }

    /* open the input file */
    if (avformat_open_input(&input_ctx, argv[1], NULL, NULL) != 0) {
        fprintf(stderr, "Cannot open input file '%s'\n", argv[2]);
        return -1;
    }

    if (avformat_find_stream_info(input_ctx, NULL) < 0) {
        fprintf(stderr, "Cannot find input stream information.\n");
        return -1;
    }

    /* find the video stream information */
    ret = av_find_best_stream(input_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &softwareDecoder, 0);
    if (ret < 0) {
        fprintf(stderr, "Cannot find a video stream in the input file\n");
        return -1;
    }
    video_stream = ret;

    if(softwareDecoder -> id == AV_CODEC_ID_H264) {
        decoder = avcodec_find_decoder_by_name("h264_v4l2m2m"); // Runs fast!
    }

    if(softwareDecoder -> id == AV_CODEC_ID_HEVC) {
        decoder = avcodec_find_decoder_by_name("hevc");  // Runs slow!
    }
    
    if(!decoder) {
		printf("decoder not found");
		return -1;
    }
	
	// Fix pixel format to DRM_PRIME
    //hw_pix_fmt = AV_PIX_FMT_DRM_PRIME;  // Starts working when I disable this!
    //hw_pix_fmt = AV_PIX_FMT_VAAPI; // test for now. Not good.

    if (!(decoder_ctx = avcodec_alloc_context3(decoder)))
        return AVERROR(ENOMEM);

    video = input_ctx->streams[video_stream];
    if (avcodec_parameters_to_context(decoder_ctx, video->codecpar) < 0)
        return -1;

    decoder_ctx->get_format  = get_hw_format;

    if (hw_decoder_init(decoder_ctx, type) < 0)
        return -1;

    if ((ret = avcodec_open2(decoder_ctx, decoder, NULL)) < 0) {
        fprintf(stderr, "Failed to open codec for stream #%u\n", video_stream);
        return -1;
    }
	
	
	
	/// actual decoding and dump the raw data
	/// this should be in OUR main loop.
    while (ret >= 0) {
        if ((ret = av_read_frame(input_ctx, &packet)) < 0)
            break;

        if (video_stream == packet.stream_index){
            ret = decode_write(decoder_ctx, &packet);
		}

			av_packet_unref(&packet);
    }
	
	
	

	/// END getdmabuf() ------------------------------------------------
	/// BEGIN create DMA texture ---------------------------------------

	EGLImageKHR dma_image;
	//~ dma_image = eglCreateImageKHR(	  
					//~ egl_display,
					//~ EGL_NO_CONTEXT,
					//~ EGL_LINUX_DMA_BUF_EXT,
					//~ NULL,
					//~ (EGLint[])
					//~ {
						//~ EGL_WIDTH, fmt.fmt.pix.width,
						//~ EGL_HEIGHT, fmt.fmt.pix.height,
						//~ EGL_LINUX_DRM_FOURCC_EXT, DRM_FORMAT_XRGB8888,  /// takes 16 or 32 bits per pixel (or 8 probably)
						//~ EGL_DMA_BUF_PLANE0_FD_EXT, expBuf_fd,
						//~ EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
						//~ EGL_DMA_BUF_PLANE0_PITCH_EXT, fmt.fmt.pix.bytesperline,
						//~ EGL_NONE
					//~ });

	if(dma_image == EGL_NO_IMAGE_KHR)
	{
		printf("error: eglCreateImageKHR failed\n");
		//return -1;
	}


	GLuint dma_texture;
	glGenTextures(1, dma_texture);
	glEnable(GL_TEXTURE_EXTERNAL_OES);
	glBindTexture(GL_TEXTURE_EXTERNAL_OES, dma_texture);
	glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, dma_image);
	glGetUniformLocation(shader_program, "texture");

	/// END create DMA texture ---------------------------------------
	
	struct v4l2_buffer buf;
	memset(&buf, 0, sizeof(buf));
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;
	buf.index = 0;

	//if(ioctl(fd, VIDIOC_QUERYBUF, &buf) == -1)
	//{
		//perror("VIDIOC_QUERYBUF");
		//return -1;
	//}

	/// Kick off the queue-dequeue cycle
	//ioctl(fd, VIDIOC_QBUF, &buf);

	/// Main Program Loop
	//  USES A LOT OF CPU EVEN WITH NO VIDEO.
	while(!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
		
		
		// video file decode and 
		//~ if ((ret = av_read_frame(input_ctx, &packet)) < 0)
            //~ break;

        //~ if (video_stream == packet.stream_index){
            //~ ret = decode_write(decoder_ctx, &packet);
		//~ }
			//~ av_packet_unref(&packet);
		
		
		//ioctl(fd, VIDIOC_DQBUF, &buf);
		//	glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, dma_image);
		//ioctl(fd, VIDIOC_QBUF, &buf);
		
		glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
		glUseProgram(shader_program);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glfwSwapBuffers(window);
	}

	glDeleteBuffers(1, &vbo);
	glfwTerminate();
	return EXIT_SUCCESS;
}
