#ifndef __INCLUDE_TEXTURE_TRANSFER_CPP
#define __INCLUDE_TEXTURE_TRANSFER_CPP
#include <iostream>
#include "TextureTransfer.h"
namespace ImageProcess
{
    #define pixSum(_X) ((int)_X.r + (int)_X.g + (int)_X.b + (int)_X.alpha)
    TextureTransfer::TextureTransfer (const Image &simg, const Image &texture, int overlap, int pattSize) : overlap(overlap), pattSize(pattSize), \
        srcImg(simg), texture(texture), result(simg.img_W, simg.img_H) {
            using std::memset;
            texSum = new double*[texture.img_H+1];
            for (int i=0; i<=texture.img_H; ++i) {
                texSum[i]=new double[texture.img_W+1];
                memset(texSum[i], 0x00, sizeof(double)*(texture.img_W+1));
            }
            srcSum = new double*[simg.img_H+1];
            for (int i=0; i<=simg.img_H; ++i) {
                srcSum[i]=new double[simg.img_W+1];
                memset(srcSum[i], 0x00, sizeof(double)*(simg.img_W+1));
            }
            for (int i=1; i<=texture.img_H; ++i) {
                double sum=0.0;
                for (int j=1; j<=texture.img_W; ++j) {
                    Pix t(texture.getPixel(j-1,i-1));
                    sum += (double)pixSum(t);
                    texSum[i][j] = texSum[i-1][j] + sum;
                }
            }
            for (int i=1; i<=simg.img_H; ++i) {
                double sum=0.0;
                for (int j=1; j<=simg.img_W; ++j) {
                    Pix t(simg.getPixel(j-1,i-1));
                    sum += (double)pixSum(t);
                    srcSum[i][j] = srcSum[i-1][j] + sum;
                }
            }
        }
    TextureTransfer::~TextureTransfer() {
        for (int i=0; i<=srcImg.img_H; ++i)
            delete[] srcSum[i];
        for (int i=0; i<=texture.img_H; ++i)
            delete[] texSum[i];
        delete[] srcSum;
        delete[] texSum;
    }
    double TextureTransfer::regionSum(double **p, int x, int y, int w, int h) const {
        return p[y+h][x+w] - p[y+h][x] - p[y][x+w] + p[y][x]; // [x, x+w); [y, y+h)
    }
    double TextureTransfer::regionErr(int tx, int ty, int sx, int sy, int w, int h) const {
        return std::fabs(regionSum(srcSum, sx, sy, w, h) - regionSum(texSum, tx, ty, w, h));
    }
    void TextureTransfer::findBest(int &bx, int &by, int sx, int sy) const  {
        using std::min;
        double minval=1e300;
        bx = by = 0;
        int Width = min((int)pattSize, (int)srcImg.img_W-sx);
        int Height= min((int)pattSize, (int)srcImg.img_H-sy);
        for (int i=0; i+Height<=texture.img_H; ++i) {
            for (int j=0; j+Width<=texture.img_W; ++j) {
                double err = regionErr(j, i, sx, sy, Width, Height);
                if (err<minval) {
                    minval = err;
                    bx = j, by = i;
                }
            }
        }
    }
    void TextureTransfer::composite(int x, int y, int sx, int sy) {
        using std::fill;
        using std::abs;
        using std::min;
        #define diff(_A, _B) abs(pixSum(_A)-pixSum(_B))
        #define of(_X,_Y) ((_X)*(overlap+2)+(_Y))
        int Width = min((int)pattSize, (int)srcImg.img_W-sx);
        int Height= min((int)pattSize, (int)srcImg.img_H-sy);
        size_t dp1Size, dp2Size;
        int *dp1, *dp2;
        #define LAZY1(XY, ___INLINE___) for (int i=1; i<XY; ++i) for (int j=1; j<=overlap; ++j) {  int minval=1e9; ___INLINE___  }
        #define LAZY2(___INLINE___) \
        { \
            int mindp=1; \
            int minv =1e9; \
            for (int i=1; i<=overlap; ++i) \
                ___INLINE___ \
        }
        Image *temp = new Image(Width, Height);
        for (int i=0; i<Height; ++i) {
            for (int j=0; j<Width; ++j) {
                temp->setPixel(j,i,result.getPixel(sx+j,sy+i));
                result.setPixel(sx+j,sy+i, texture.getPixel(x+j, y+i));
            }
        }
        if (sy!=0) {
            dp1Size = Width*(overlap+2);
            dp1 = new int[dp1Size];
            fill(dp1, dp1+dp1Size, 1e9);
            for (int i=0; i<overlap; ++i) {
                Pix rp1(temp->getPixel(0, i));
                Pix tp1(texture.getPixel(x, y+i));
                dp1[i+1] = diff(rp1, tp1);
            }
            LAZY1( Width, 
                    for (int k=-1; k<=1; ++k)
                        minval=min(minval, dp1[of(i-1,j+k)]);
                    Pix r(temp->getPixel(i, j-1));
                    Pix t(texture.getPixel(x+i, y+j-1));
                    dp1[of(i,j)] = minval + diff(r,t);
                 )
            LAZY2(if (dp1[of(Width-1,i)]<minv) {
                      minv = dp1[of(Width-1,i)];
                      mindp= i;
                  }
                  for (int i=Width-1; i>=1; --i) {
                      Pix r(temp->getPixel(i, mindp-1));
                      Pix t(texture.getPixel(x+i, y+mindp-1));
                      int val = diff(r,t);
                      for (int j=-1; j<=1; ++j) {
                          if (dp1[of(i-1, mindp+j)]+val==dp1[of(i,mindp)]) {
                              dp1[of(i, mindp)] = INT_MIN;
                              mindp += j; break;
                          }
                      }
                  }
                  dp1[of(0,mindp)]=INT_MIN;
                 )
            for (int i=0; i<Width; ++i) {
                for (int j=0; j<Height; ++j) {
                    if (dp1[of(i, j+1)]==INT_MIN) break;
                    result.setPixel(sx+i,sy+j, temp->getPixel(i,j));
                }
            }
            delete[] dp1;
        }
        if (sx!=0) {
            dp2Size = Height*(overlap+2);
            dp2 = new int[dp2Size];
            fill(dp2, dp2+dp2Size, 1e9);
            for (int i=0; i<overlap; ++i) {
                Pix rp2(temp->getPixel(i, 0));
                Pix tp2(texture.getPixel(x+i, y));
                dp2[i+1] = diff(rp2, tp2);
            }
            LAZY1( Height,
                    for (int k=-1; k<=1; ++k) 
                        minval=min(minval, dp2[of(i-1,j+k)]);
                    Pix r(temp->getPixel(j-1, i));
                    Pix t(texture.getPixel(x+j-1,  y+i));
                    dp2[of(i,j)] = minval + diff(r,t);
                 )
            LAZY2(if (dp2[of(Height-1,i)]<minv) {
                      minv = dp2[of(Height-1,i)];
                      mindp= i;
                  }
                  for (int i=Height-1; i>=1; --i) {
                      Pix r(temp->getPixel(mindp-1, i));
                      Pix t(texture.getPixel(x+mindp-1, y+i));
                      int val = diff(r,t);
                      for (int j=-1; j<=1; ++j) {
                          if (dp2[of(i-1, mindp+j)]+val==dp2[of(i,mindp)]) {
                              dp2[of(i, mindp)] = INT_MIN;
                              mindp += j; break;
                          }
                      }
                  }
                  dp2[of(0,mindp)]=INT_MIN;
                 )
            for (int i=0; i<Height; ++i) {
                for (int j=0; j<Width; ++j) {
                    if (dp2[of(i, j+1)]==INT_MIN) break;
                    result.setPixel(sx+j,sy+i, temp->getPixel(j,i));
                }
            }
            delete[] dp2;
        }
        
        #undef LAZY1
        #undef LAZY2
        #undef of
        #undef diff
        delete temp;
    }
    void TextureTransfer::montage(void) {
        int step = pattSize-overlap;
        for(int i=0;;i+=step) {
            for (int j=0;;j+=step) {
                int bx=0, by=0;
                findBest(bx,by,j,i);
                composite(bx,by,j,i);
                if (j+pattSize>=srcImg.img_W) break; // 填滿了
            }
            if (i+pattSize>=srcImg.img_H) break; // 填滿了
        }
    }
    Image* TextureTransfer::run(void) {
        this->montage();
        return new Image(this->result);
    }
};
#undef pixSum
#endif
