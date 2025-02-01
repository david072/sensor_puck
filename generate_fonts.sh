#!/bin/bash

settings=("--bpp" "2" "--no-compress" "--use-color-info" "--format" "lvgl")
# ascii + degree symbol ('°')
range=("0-0x,0xB0")

output_prefix="./main/ui/fonts/font_montagu_slab"
regular_font="./font_montagu_slab/MontaguSlab-VariableFont_regular,opsz.ttf"
bold_font="./font_montagu_slab/MontaguSlab-VariableFont_bold,opsz.ttf"

generate_font ()
{
  font=""
  output_suffix=""
  case $2 in
    "regular") font=$regular_font ;;
    "bold")
      font=$bold_font
      output_suffix="_bold" ;;
    *)
      echo "Invalid font weight $2"
      exit 1 ;;
  esac

  lv_font_conv --font $font -r ${range[@]} --size $1 -o "${output_prefix}_$1${output_suffix}.c" ${settings[@]}
}

# headline1
generate_font 40 "regular"
# headline2
generate_font 32 "bold"
# headline3
generate_font 24 "regular"
# body
generate_font 20 "regular"
# caption
generate_font 16 "regular"

exit 0
