#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <alsa/asoundlib.h>


#define PLAYBACK_FILE "R48K-16LE-CH1.pcm"
#define CAPTURE_FINE "capture.pcm"

#define PCM_NAME	"hw:CARD=mycodec,DEV=0"
#define RATE 		48000
#define FORMAT		SND_PCM_FORMAT_S16_LE
#define CHANNELS	1

snd_pcm_hw_params_t *hw_params;

int print_all_pcm_name(void) {
	char **hints;
	/* Enumerate sound devices */
	int err = snd_device_name_hint(-1, "pcm", (void***)&hints);
	if(err != 0)
	   return -1;

	char** n = hints;
	while(*n != NULL) {
		char *name = snd_device_name_get_hint(*n, "NAME");

		if(name != NULL && 0 != strcmp("null", name)) {
			printf("pcm name : %s\n",name);
			free(name);
		}
		n++;
	}

	snd_device_name_free_hint((void**)hints);
	return 0;
}

snd_pcm_t *open_sound_dev(snd_pcm_stream_t type,const char *name, unsigned int rate, int format,int channels,snd_pcm_uframes_t *period_frames) {
	int err;
	snd_pcm_t *handle;
	int dir = 0;
	
	printf("rate=%d, format=%d, channels=%d\n",rate,format,channels);

	if((err = snd_pcm_open(&handle, name, type, 0)) < 0) {
		return NULL;
	}
	   
	if((err = snd_pcm_hw_params_malloc(&hw_params)) < 0) {
		printf("cannot allocate hardware parameter structure (%s)\n",
			 snd_strerror(err));
		return NULL;
	}
			 
	if((err = snd_pcm_hw_params_any(handle, hw_params)) < 0) {
		printf("cannot initialize hardware parameter structure (%s)\n",
			 snd_strerror(err));
		return NULL;
	}

	if((err = snd_pcm_hw_params_set_access(handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
		printf("cannot set access type (%s)\n",
			 snd_strerror(err));
		return NULL;
	}

	if((err = snd_pcm_hw_params_set_format(handle, hw_params, format)) < 0) {
		printf("cannot set sample format (%s)\n",
			 snd_strerror(err));
		return NULL;
	}

	if((err = snd_pcm_hw_params_set_rate_near(handle, hw_params, &rate, 0)) < 0) {
		printf("cannot set sample rate (%s)\n",
			 snd_strerror(err));
		return NULL;
	}

	if((err = snd_pcm_hw_params_set_channels(handle, hw_params, channels)) < 0) {
		printf("cannot set channel count (%s)\n",
			 snd_strerror(err));
		return NULL;
	}

	if((err = snd_pcm_hw_params(handle, hw_params)) < 0) {
		printf("cannot set parameters (%s)\n",
			 snd_strerror(err));
		return NULL;
	}
	
	if(period_frames != NULL) {
		//获取一个周期有多少帧数据
		if((err =snd_pcm_hw_params_get_period_size(hw_params, period_frames, &dir)) < 0){
			printf("cannot get period size (%s)\n",
				snd_strerror(err));
			return NULL;
		}
	}
	
	snd_pcm_hw_params_free(hw_params);

	return handle;
}


int main(void) {
	int p,err;
	snd_pcm_t *playback_handle;
	snd_pcm_t *capture_handle;
	int pfd,cfd;
	snd_pcm_uframes_t period_frames;
	int size2frames;
	
	printf("program running ...\n");
	
	//查看所有pcm的name
	//print_all_pcm_name();
	
	playback_handle = open_sound_dev(SND_PCM_STREAM_PLAYBACK,PCM_NAME,RATE,FORMAT,CHANNELS,&period_frames);
	if(!playback_handle) {
		printf("cannot open for playback\n");
        return -1;
    }
	
	usleep(5);
	
	capture_handle = open_sound_dev(SND_PCM_STREAM_CAPTURE,PCM_NAME,RATE,FORMAT,CHANNELS,NULL);
	if(!capture_handle) {
		printf("cannot open for capuure\n");
		snd_pcm_close(playback_handle);
        return -1;
    }
	
	if((err = snd_pcm_prepare(playback_handle)) < 0) {
		printf("cannot prepare audio interface for use (%s)\n",
			 snd_strerror(err));
		goto out;
	}

	if((err = snd_pcm_prepare(capture_handle)) < 0) {
		printf("cannot prepare audio interface for use (%s)\n",
			 snd_strerror(err));
		goto out;
	}
	
	//打开要播放的PCM文件
	pfd = open(PLAYBACK_FILE,O_RDONLY,0644);
	if(pfd < 0){
		printf("open %s error!!!\n",PLAYBACK_FILE);
		goto out;
	}
	
	//新建一个进程, 子进程播放, 父进程录音
	p = fork();
	if(p < 0) {
		printf("fork error!!!\n");
		goto out;
	}
	
	if(p==0) {
		char *pbuf;
		int size,period_bytes;
		period_bytes = snd_pcm_frames_to_bytes(playback_handle,period_frames);
		pbuf = malloc(period_bytes);
		while( size = read(pfd, pbuf, period_bytes)) {
			//解决最后一个周期数据问题
			if(size < period_bytes) {
				memset(pbuf+size, 0, period_bytes-size);
			}
			
			//size2frames = snd_pcm_bytes_to_frames(playback_handle,size);
			size2frames = period_frames;
			
			//向PCM写入数据，播放
			err = snd_pcm_writei(playback_handle, pbuf, size2frames);
			if(err != size2frames) {
				printf("write to audio interface failede err:%d (size2frames:%d)\n",err,size2frames);
				free(pbuf);
				close(pfd);
				exit(-1);
			}
			printf("process:playback wrote %d frames\n",size2frames);
			usleep(100);
		}
		
		free(pbuf);
		close(pfd);
		sleep(1);	//等待一下，给点时间父进程录音
		exit(0);
	}
	
	char *cbuf;
	const int frames_size = snd_pcm_frames_to_bytes(capture_handle,period_frames);
	cbuf = malloc(frames_size);
	memset(cbuf, 0, frames_size);
	
	//打开录音的保存文件
	cfd = open(CAPTURE_FINE,O_RDWR | O_TRUNC | O_CREAT,0644);
	if(cfd < 0){
		printf("open %s error!!!\n",CAPTURE_FINE);
		goto out;
	}
	
	while(waitpid(p, NULL, WNOHANG) == 0) {	//查看一下子进程是否已经退出
		//向PCM读一周期数据
		if((size2frames = snd_pcm_readi(capture_handle, cbuf, period_frames)) < 0) {
			printf("read from audio interface failed (%d)\n",size2frames);
			free(cbuf);
			close(cfd);
			goto out;
		}
		printf("--process:capture read %d frames\n",size2frames);
		write(cfd,cbuf,snd_pcm_frames_to_bytes(capture_handle,size2frames));
		memset(cbuf,0,frames_size);
		usleep(100);
	}
	free(cbuf);
	close(cfd);

	
out:
	snd_pcm_close(playback_handle);
	snd_pcm_close(capture_handle);
	printf("program finish ...\n");
	return 0;
}