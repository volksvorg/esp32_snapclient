// ESP32 Server with static HTML page

#include "tools.h"


void tools_split_string(char* str, int size){
	int x=0,y=0,z=0;
	int position[10];
	char tmp1[40];
	char tmp2[20];
	memset( tmp1, 0x00, 40 );
	memset( tmp2, 0x00, 20 );
	for(x=0;x<10;x++){
		position[x]=0x00;
	}
	for(x=0;x<size;x++){
		if(str[x]=='%'){
			position[y++]=x+1;
			z++;
		}
	}
	z+=1;
	for(y=0;y<z;y++){
		int s=0;
		if(y==0){
			s= position[0];
			strncpy(tmp2, str, s-1);
			strcat(tmp1,tmp2);
			strcat(tmp1,"/");
			memset( tmp2, 0x00, 20 );
		}else{
			s=position[y] - position[y-1];
			if(s>=0){
				int a=position[y -1] +2;
				strncpy(tmp2,str + a ,s-3);
				strcat(tmp1,tmp2);
				strcat(tmp1,"/");
				memset( tmp2, 0x00, 20 );
			}else{
				int a=position[y -1] +2;
				strcpy(tmp2,str + a);
				strcat(tmp1,tmp2);
				memset( tmp2, 0x00, 20 );
			}
		}
	}
	memset( str, 0x00, size);
	strcpy(str,tmp1);
}



int tools_set_audio_volume(audio_board_handle_t bh, int volume, int muted){
	volume/=5;
	volume+=80;
	if(muted==1){
		audio_hal_set_volume(bh->audio_hal, volume);
	}else{
		audio_hal_set_volume(bh->audio_hal, 0);
	}
	return volume;
}

int tools_set_eq_gain(audio_element_handle_t self, int *gain){
	for(int x=0;x<10;x++){
		if (gain[x]>13){
			gain[x]=13;
		}
		if(gain[x]<-13){
			gain[x]=-13;
		}
		equalizer_set_gain_info(self, x, gain[x], true);
	}
	return 0;
}




