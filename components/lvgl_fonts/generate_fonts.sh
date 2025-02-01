#!/bin/bash

settings=("--bpp" "2" "--no-compress" "--use-color-info" "--format" "lvgl")
# ascii + degree symbol ('°')
range=("0-0x80,0xB0")

output_prefix="font_montagu_slab"
regular_font="./fonts/MontaguSlab-VariableFont_regular,opsz.ttf"
bold_font="./fonts/MontaguSlab-VariableFont_bold,opsz.ttf"

# arguments to merge FontAwesome symbols into the font
# for builtin symbols see https://lvgl.io/tools/fontconverter ("Useful notes")
symbols_range=$(
  cat << EOM
61441,61448,61451,61452,61452,61453,61457,61459,61461,61465,61468,61473,61478,61479,\
61480,61502,61512,61515,61516,61517,61521,61522,61523,61524,61543,61544,61550,61552,\
61553,61556,61559,61560,61561,61563,61587,61589,61636,61637,61639,61671,61674,61683,\
61724,61732,61787,61931,62016,62017,62018,62019,62020,62087,62099,62212,62189,62810,\
63426,63650
EOM
)

symbols_args=("--font" "./fonts/lvgl_FontAwesome5.woff" "-r" $symbols_range)

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

  output="${output_prefix}_$1${output_suffix}.c"

  if [ ! -f $output ]; then
    lv_font_conv --font $font -r ${range[@]} ${symbols_args[@]} --size $1 -o $output ${settings[@]}
  fi

  echo $output
}

fonts=(
  # headline1
  $(generate_font 40 "regular")
  # headline2
  $(generate_font 32 "bold")
  # headline3
  $(generate_font 24 "regular")
  # body
  $(generate_font 20 "regular")
  # caption
  $(generate_font 16 "regular")
)

printf "%s\n" "${fonts[@]}"
exit 0
