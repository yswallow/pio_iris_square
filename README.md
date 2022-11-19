# RP2040とマジョカアイリス液晶(正方形)でBad Apple!

## 使用方法

```
(put video file as bad_apple.mp4)
(put uf2conv.py from https://github.com/microsoft/uf2/raw/master/utils/uf2conv.py)

$ mkdir bmp
$ ffmpeg -i bad_apple.mp4 -vf scale=128:-1 bmp/%04d.bmp
$ gcc converter.c
$ ./a.out

( data.bin generated )

$ python uf2conv.py -b 0x10010000 -o bad_apple_data.uf2 -i data.bin -f 0xe48bff56 -c

(copy bad_apple_data.uf2 to RP2-BOOT drive)

$ PICO_SDK_PATH={{path to raspberrypi/pico-sdk}}
$ mkdir build
$ cd build
$ cmake .. && make

(copy bad_apple.uf2 to RPI-RP2 drive)
```

## 配線

| 液晶 | RP2040 |
|----|------|
| 1  | GND  |
| 2  | IO14 |
| 3  | GND  |
| 4  | IO2  |
| 5  | IO3  |
| 6  | IO4  |
| 7  | IO5  |
| 8  | IO6  |
| 9  | IO7  |
| 10 | IO8  |
| 11 | IO9  |
| 12 | GND  |
| 13 | 3V3  |
| 14 | IO15 |
| 15 | GND  |
| 16 | IO10 |
| 17 | Open |
| 18 | 3V3  |
| 19 | 3V3  |
| 20 | VBUS+27Ω |
| 21 | GND  |
| 22 | GND  |

## 知見

* DMAのデータが化けることがある
* PIOのSETは5ビット

### DMAのデータが化ける

DREQがCPUクロックに対して頻繁すぎるとDMAするデータが化ける。バッファを大きくするなり
動作を遅くするなりしてペリフェラルのデータ要求頻度を減らそう。

### PIOのSETは5ビット

`SET pins 0;` しても6ピン目以降はLOWにならない。

## 必要な技術

* データ圧縮
* Flashへのプログラム以外の書き込み
* RP2040のオーバークロック
* PIOで効率よく送る設計

### データ圧縮

初めは幅128pxで各ピクセルを2値で記録しようと思ったが, 128*96/8*6571でおよそ12MBになるため圧縮が必要になった。
液晶サイズと同じ幅132pxでしようともしたが, 圧縮後のデータが2MBを超えるため断念した。
幅128px*高さ96pxにしたあと圧縮する。

`ffmpeg -i bad_apple.mp4 -vf scale=128:-1 bmp/%04d.bmp`

24bitカラーのBMPに変換して1ピクセルずつ読んでいく。
フレームの始めを黒として, 白黒それぞれの色の継続区間を1バイト(0〜255)ずつ記録するようにした。255px以上続く場合は一度1バイト0を入れる。
1.8MBになった。
幅128pxであることもあり初めは4ビットずつにしていたが, 4ビットではファイルサイズが2MBを超えた。8ビットずつにすると1.8MBになった。
フレームごとにセパレータを入れたいなら2バイト0を入れればいいと思う。この方式では0が2回続くことはないので。

### Flashへのプログラム以外の書き込み

binをUF2に変換するだけ。hexにしてプログラムと結合したあとにUF2に変換してもよい。
プログラムのUF2ファイルが28KBなので64KBオフセットした。
実際にFlashに書き込まれるサイズはUF2のファイルサイズの半分なので16KBのオフセットでもいけると思う。

`python ~/prog/uf2/utils/uf2conv.py -b 0x10010000 -o bad_apple_data.uf2 -i data.bin -f 0xe48bff56 -c`

Flashからのmemcpyは遅いかと思ったがそうでもなさそうだ。

### RP2040のオーバークロック

速すぎるとPIOのクロックを遅くしてもDMAのときのようにあとのほうの表示が乱れる。
すべての周波数が可能というわけではないので与えられたPythonプログラムで調べてからプログラムを書き換える。
184MHzでは表示が乱れたので182MHzにした。

```
$ python ~/prog/pico-sdk/src/rp2_common/hardware_clocks/scripts/vcocalc.py 182
Requested: 182.0 MHz
Achieved: 182.0 MHz
FBDIV: 91 (VCO = 1092 MHz)
PD1: 6
PD2: 1
```

#### DMAへの転換

DMAがうまく動かないのはDREQの間隔が短すぎるためだと気づき, 
PIOのclkdivを設定してPIOを遅くしたところDMAでもデータが化けることなく転送できるようになった。
そこでCPUを225MHzまでオーバークロックし, PIOのclkdivを3.5にすることでDMAを使用するようにした。
pull間隔はCPUクロック(PIOの命令数×clkdiv)で12以上にしたほうが良いようだ。

マルチコアでFIFOをやり取りするより2倍速くなった。

### PIOで効率よく送る設計

8ビットのデータに加えてDC(DATA/COMMAND)の1ビットもPIOに送信するようにした。
2枚の液晶でデータ線を共有するならCSもPIOに任せるといいと思う。
「0だとポート1を, 1ならポート2をHIGHにする」という動作はPIOには難しいのでCSに2ビット使うことになると思う。
番号が連続したGPIOポートに接続する必要がある。小型ボードでは難しいかもしれない。

