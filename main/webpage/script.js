const dest_height = 480;
const dest_width  = 800;
const ratio       = dest_width / dest_height;

const canvas = document.querySelector("#img_result");
const canvas_context = canvas.getContext("2d", {willReadFrequently: true});

const img_preview = document.querySelector("#img_original");

// The output array will encode colors on two bits: 0x0 for black, 0x1 for white, 0x2 for red
var output_array = new Uint8ClampedArray((dest_height*dest_width)/4);

function handleFileSelect(evt) {
    var file = document.querySelector("#img_upload").files[0];
    var reader = new FileReader();

    reader.onload = function () {
            var img = new Image;

            img.onload = function() {
                // Draw the original image into a clean canvas
                canvas_context.clearRect(0, 0, canvas.width, canvas.height);         

                canvas.height = img.height;
                canvas.width = img.width;

                canvas_context.drawImage(img, 0, 0);
                
                // Show the original image preview
                img_preview.src = canvas.toDataURL();

                let dx = 0;
                let dy = 0;

                // Portrait image, to be rotated around its center
                if (img.width < img.height)
                {
                    canvas.height = img.width;
                    canvas.width = img.height;
                    canvas_context.translate(canvas.width/2, canvas.height/2);
                    canvas_context.rotate(Math.PI/2);
                    dx = -img.width/2;
                    dy = -img.height/2;
                }

                // Draw image to canvas
                canvas_context.drawImage(img, dx, dy);

                // Crop in the x axis
                let clip_width = canvas.width;
                let clip_height = canvas.height;

                if (canvas.width > (canvas.height * ratio))
                {
                    clip_width = canvas.height * ratio;
                }
                // Crop on the y axis
                else
                {
                    clip_height = 1/ratio * canvas.width;
                }

                let clip_x = (canvas.width - clip_width) / 2;
                let clip_y = (canvas.height - clip_height) / 2;

                // Get cropped area from canvas
                let resulting_img = canvas_context.getImageData(clip_x, clip_y, clip_width, clip_height);

                let tmp_canvas = document.createElement("canvas");
                let tmp_canvas_context = tmp_canvas.getContext("2d");

                tmp_canvas.width = clip_width;
                tmp_canvas.height = clip_height;

                tmp_canvas_context.putImageData(resulting_img, 0, 0);

                // Now scale image to target sizes
                canvas_context.resetTransform();
                canvas.height = dest_height;
                canvas.width = dest_width;
                
                canvas_context.scale(dest_width/clip_width, dest_height/clip_height);
                canvas_context.drawImage(tmp_canvas, 0, 0);
                
                // Quantize image to black-white-red
                
                // Get raw pixels
                resulting_img = canvas_context.getImageData(0, 0, dest_width, dest_height);
                const pixels = resulting_img.data;

                let output_array_idx = 0;
                let output_array_sub = 0;

                // Each pixel is RGBA
                for (let y = 0; y < dest_height; y++) {
                    for (let x = 0; x < dest_width; x++) {
                        // Get the pixel index
                        let i = y*dest_width*4 + x*4;

                        pixels[i+3] = 0xFF; // Set alpha to max

                        // Compute colors ratio
                        const gb = Math.sqrt(Math.pow(pixels[i + 1], 2) + Math.pow(pixels[i + 2],2));
                        const rgb = Math.sqrt(Math.pow(pixels[i], 2) + Math.pow(pixels[i + 1], 2) + Math.pow(pixels[i + 2],2));

                        // Reduce channels to black and white, from grayscale
                        let newpixel_gb = (gb > 128) ? 255 : 0;
                        let newpixel_r  = (rgb > 128) ? 255 : 0;

                        // If red is major, set it full blast and no black channel
                        if ((pixels[i] >= 128) && (gb < 128))
                        {
                            newpixel_r  = 0xFF;
                            newpixel_gb = 0;
                            // Store the nibbles
                            output_array[output_array_idx] |= 0x2 << output_array_sub;
                        }
                        else
                        {
                            output_array[output_array_idx] |= (newpixel_gb >> 7) << output_array_sub;
                        }

                        // Increment sub-iterator and iterator
                        output_array_sub = (output_array_sub+2) % 8;
                        output_array_idx = (output_array_sub == 0) ? (output_array_idx+1) : output_array_idx;

                        // Set pixels
                        pixels[i] = newpixel_r;
                        pixels[i + 1] = newpixel_gb;
                        pixels[i + 2] = newpixel_gb;


                        // Compute quantization errors
                        error_gb = gb - newpixel_gb;
                        error_r = Math.sqrt(Math.pow(pixels[i]) - Math.pow(newpixel_r));

                        // Floyd-Steinberg dithering
                        if ((x+1) < dest_width)
                        {         
                            let right = y*dest_width*4 + (x+1)*4;
                            pixels[right] += (error_r * 7) >> 4;
                            pixels[right + 1] += (error_gb * 7) >> 4;
                            pixels[right + 2] += (error_gb * 7) >> 4;
                        }
                        
                        if ((y+1) != dest_height)
                        {
                            if (x > 0)
                            {
                                let bottomleft = (y+1)*dest_width*4 + (x-1)*4;
                                pixels[bottomleft] += (error_r *  3) >> 4;
                                pixels[bottomleft + 1] += (error_gb * 3) >> 4;
                                pixels[bottomleft + 2] += (error_gb * 3) >> 4;
                            }
                            
                            let bottom = (y+1)*dest_width*4 + x*4;
                            pixels[bottom] += (error_r * 5) >> 4;
                            pixels[bottom + 1] += (error_gb * 5) >> 4;
                            pixels[bottom + 2] += (error_gb * 5) >> 4;
                            
                            if ((x+1) < (dest_width-1))
                            {
                                let bottomright = (y+1)*dest_width*4 + (x+1)*4;
                                pixels[bottomright] += error_r >> 4;
                                pixels[bottomright + 1] += error_gb >> 4;
                                pixels[bottomright + 2] += error_gb >> 4;
                            }
                        }
                    }
                }
                
                // Clear canvas and set its contents to the quantized pixels
                canvas_context.clearRect(0, 0, canvas.width, canvas.height);
                canvas_context.putImageData(resulting_img, 0, 0);
            };

            img.src = reader.result;
    };
        
    if (file) {
        reader.readAsDataURL(file);
    }
}
    
document.getElementById("img_upload").addEventListener("change", handleFileSelect, false);
