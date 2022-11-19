#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define PRINT 0

int main(void) {
    FILE *bmp, *bin;
    char filename[16];
    uint8_t pix[3];
    size_t ret;
    
    bin = fopen("data.bin", "wb");
    if( bin==NULL ) {
        printf("cannot create file\n");
        return -1;
    }
    
    for(uint16_t frame=1; frame<6572; frame++) {
        sprintf(filename, "bmp/%04d.bmp", frame);
        bmp = fopen(filename, "rb");
        
        if(bmp==NULL) {
            printf("break on %d\n", frame);
            fclose(bin);
            return 0;
        }
        
        fseek(bmp, 54, SEEK_SET);
        
        bool prev_pixel = 0;
        uint8_t count = 0;
        uint8_t zero = 0;
        
        for(uint16_t i=0; i<128*96; i++) {
            bool pixel;
#if PRINT
            if(i%128==0) {
                printf("\n");
            }
#endif
            fread(&pix, 1, 3, bmp);
            
            if( pix[0]+pix[1]+pix[2] < 0x180 ) {
                pixel = 0;
            } else {
                pixel = 1;
            }
#if PRINT            
            printf(pixel ? "*" : " ");
#endif
            if( prev_pixel==pixel ) {
                if(count==255) {
                    fwrite(&count, 1, 1, bin);
                    fwrite(&zero, 1, 1, bin);
                    count = 1;
                } else {
                    count++;
                }
            } else {
                fwrite(&count, 1, 1, bin);
                
                prev_pixel = pixel;
                count = 1;
           }
        } // end pixel
        fclose(bmp);
        
        fwrite(&count, 1, 1, bin);
        count = 0;
    } // end frame
    fclose(bin);
    return 0;
}
