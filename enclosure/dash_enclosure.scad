// Enclosure for the Waveshare ESP32-S3-Touch-LCD-3.5 with an 8-pixel NeoPixel
// bar above the screen.
//
//   part = "assembly"  : both parts together, cut open for a preview
//   part = "front"     : print this one face down (front face on the bed)
//   part = "back"      : print this one flat on its floor
//
// Coordinates: origin at the centre of the glass, X to the right and Y up as
// seen from the front, Z toward the viewer. Z = 0 is the front face of the
// glass; the bezel plate is in front of it (0..face), the board is behind it
// (negative Z).
//
// Parts you need: 4 x M2 screws for the board (see "Board mounting" below),
// 2 x M2 x 4 screws for the NeoPixel bar, 4 x M3 x 12 screws to close the case (they self-tap into the front bosses).

part = "assembly";   // "assembly", "front" or "back"

$fn = 48;
eps = 0.01;

// ---------------------------------------------------------------- the screen
glass_w  = 92.44;
glass_h  = 61.0;
glass_r  = 6.0;
glass_t  = 1.1;
glass_clear = 0.3;       // gap around the glass in its pocket

view_w   = 73.44;        // visible area
view_h   = 48.96;
view_margin = 0.6;       // the window is this much larger than the visible area on each side

hole_dx  = 72.0;         // M2 holes, centred on the glass
hole_dy  = 48.5;

board_back_z = -8.6;     // back face of the board / its standoffs (glass 1.1 + 7.5)
board_total  = 11.5;     // glass front to the highest part (GPIO header) at the back

// USB-C: on the right edge when seen from the front, centred vertically.
usb_y = 0;
usb_w = 10.5;            // cut-out size (connector is about 9 x 3.3)
usb_h = 5.0;
usb_z = -6.8;            // centre height of the connector

// -------------------------------------------------------------- NeoPixel bar
// Measured from a photo of the bar next to a ruler (about +-0.3 mm). LED side
// faces the front; the PCB lies against the back of the front face and the
// LEDs go into their holes.
px_count    = 8;
px_pitch    = 6.5;       // LED centre to LED centre
px_hole     = 5.4;       // square hole per pixel (the LED package is 5.0 mm)
px_hole_r   = 0.6;       // corner radius of the pixel holes
diffuser_skin = 0.0;     // >0 leaves a thin front skin the light shines through
                         // (print in white); 0 = open holes
pcb_len     = 53.5;
pcb_up      = 3.5;       // PCB edge above the LED row centre line
pcb_down    = 5.8;       // PCB edge below the LED row centre line
pcb_t       = 1.6;       // PCB thickness (assumed)
pcb_hole_d  = 2.8;       // its two mounting holes
pcb_hole_dx = 25.7;      // spacing between them
pcb_hole_dy = 4.1;       // below the LED row centre line
pin_d       = 2.5;       // locating pin through each PCB hole
pin_pilot   = 1.6;       // M2 self-tap pilot in the pin and the front face
pin_len     = 1.2;       // pin length behind the face (less than pcb_t, so the
                         // screw head clamps the PCB)
px_clear    = 0.6;       // free space around the PCB

// ---------------------------------------------------------------- the shell
wall      = 2.4;
face      = 3.0;         // front plate thickness (in front of the glass)
side_marg = 6.0;         // material beside and below the glass pocket (incl. wall)
strip_zone = 15.0;       // material above the glass pocket (holds the bar)
outer_r   = 6.0;
split_z   = -5.0;        // where the front and back parts meet
floor_t   = 2.4;
back_clear = 1.5;        // room behind the tallest part (header)

pocket_w = glass_w + 2 * glass_clear;
pocket_h = glass_h + 2 * glass_clear;

x_half = pocket_w / 2 + side_marg;
y_bot  = -(pocket_h / 2 + side_marg);
y_top  =  pocket_h / 2 + strip_zone;
outer_w = 2 * x_half;
outer_h = y_top - y_bot;
outer_cy = (y_top + y_bot) / 2;

back_z = -(board_total + back_clear) - floor_t;   // outside of the back floor

// joint between the two parts: the front part has a lip, the back part a rebate
lip_h   = 2.0;
lip_out = 1.3;           // wall thickness left on the back part at the rebate
lip_gap = 0.15;
lip_t   = 1.5;

// closing screws (M3): in the four corners
boss_in = 4.6;           // distance of the screw axis from the outer edges
boss_r  = 3.8;
screw_pilot = 2.5;       // self-tap pilot in the front boss
screw_clear = 3.5;       // clearance through the back part
screw_head_d = 6.2;      // pocket for the M3 pan head (head is about 5.6 x 2.4)
screw_head_depth = 3.2;
boss_top = face - 0.8;   // the pilot hole stops here, below the front face

bosses = [for (sx = [-1, 1], sy = [-1, 1])
          [sx * (x_half - boss_in), sy > 0 ? y_top - boss_in : y_bot + boss_in]];

// ------------------------------------------------------ board mounting (M2)
// The four M2 screws go through posts in the back part and thread into the
// M2 mounting points on the back of the board. Post tops touch the board back
// (board_back_z). If your board needs the screws to enter from another side,
// change this block only.
post_r      = 3.4;
post_hole   = 2.3;       // M2 clearance
post_head_d = 4.8;       // pocket for the M2 pan head (about 3.8 x 1.6)
post_head_depth = 2.0;
post_pos = [for (sx = [-1, 1], sy = [-1, 1]) [sx * hole_dx / 2, sy * hole_dy / 2]];

// ------------------------------------------------------------------ helpers
module rrect(w, h, r, cx = 0, cy = 0) {
    translate([cx, cy]) offset(r = r) square([w - 2 * r, h - 2 * r], center = true);
}

module outer2d() { rrect(outer_w, outer_h, outer_r, 0, outer_cy); }

px_y = pocket_h / 2 + pcb_down + px_clear + 1.4;  // Y of the LED row centre line
px_x = [for (i = [0 : px_count - 1]) (i - (px_count - 1) / 2) * px_pitch];
pin_pos = [[-pcb_hole_dx / 2, px_y - pcb_hole_dy], [pcb_hole_dx / 2, px_y - pcb_hole_dy]];

// ------------------------------------------------------------------- front
module front_part() {
    difference() {
        union() {
            // shell: face plate + walls
            difference() {
                translate([0, 0, split_z]) linear_extrude(face - split_z) outer2d();
                // cavity behind the face plate
                translate([0, 0, split_z - eps])
                    linear_extrude(-split_z + eps)
                        offset(delta = -wall) outer2d();
            }
            // corner bosses for the closing screws
            for (b = bosses)
                translate([b[0], b[1], split_z]) cylinder(r = boss_r, h = face - split_z);
            // lip that slides into the rebate of the back part
            translate([0, 0, split_z - lip_h]) linear_extrude(lip_h + eps)
                difference() {
                    offset(delta = -(lip_out + lip_gap)) outer2d();
                    offset(delta = -(lip_out + lip_gap + lip_t)) outer2d();
                    for (b = bosses) translate(b) circle(r = boss_r + 0.4);
                }
            // ring that locates the glass sideways
            translate([0, 0, -3.0]) linear_extrude(3.0 + eps)
                rrect(pocket_w + 2 * 1.2, pocket_h + 2 * 1.2, glass_r + glass_clear + 1.2);
            // locating pins for the NeoPixel PCB (they go through its holes)
            for (p = pin_pos)
                translate([p[0], p[1], -pin_len]) cylinder(d = pin_d, h = pin_len + eps);
        }
        // glass pocket (from the back of the face plate, so a lip holds the glass)
        translate([0, 0, -(glass_t + glass_clear + 2.0)])
            linear_extrude(glass_t + glass_clear + 2.0 + eps)
                rrect(pocket_w, pocket_h, glass_r + glass_clear);
        // screen window through the face plate
        translate([0, 0, -eps]) linear_extrude(face + 2 * eps)
            rrect(view_w + 2 * view_margin, view_h + 2 * view_margin, 2.0);
        // one hole per NeoPixel (leaves a thin skin if diffuser_skin > 0)
        for (x = px_x)
            translate([x, px_y, -eps])
                linear_extrude(face - diffuser_skin + 2 * eps)
                    rrect(px_hole, px_hole, px_hole_r);
        // M2 pilot holes in the pins and the front face (the screw stops short of the front)
        for (p = pin_pos)
            translate([p[0], p[1], -pin_len - eps])
                cylinder(d = pin_pilot, h = pin_len + face - 0.6 + eps);
        // seat for the PCB: it lies flat against the back of the face
        // (nothing to cut, the face back is at Z = 0)
        // pilot holes for the closing screws
        for (b = bosses)
            translate([b[0], b[1], split_z - eps])
                cylinder(d = screw_pilot, h = boss_top - split_z + eps);
        // (the lip is added below, outside this difference)
        // USB-C opening, right side
        usb_cut();
    }
}

module usb_cut() {
    translate([x_half - wall - 1, usb_y - usb_w / 2, usb_z - usb_h / 2])
        cube([wall + 2, usb_w, usb_h]);
}

// -------------------------------------------------------------------- back
module back_part() {
    difference() {
        back_body();
        // closing screws: clearance hole right through the floor and the column,
        // with a pocket for the head on the outside
        for (b = bosses) {
            translate([b[0], b[1], back_z - eps])
                cylinder(d = screw_clear, h = split_z - back_z + 2 * eps);
            translate([b[0], b[1], back_z - eps])
                cylinder(d = screw_head_d, h = screw_head_depth + eps);
        }
        // board screws: through the floor and the post, pocket for the head
        for (p = post_pos) {
            translate([p[0], p[1], back_z - eps])
                cylinder(d = post_hole, h = board_back_z - back_z + 2 * eps);
            translate([p[0], p[1], back_z - eps])
                cylinder(d = post_head_d, h = post_head_depth + eps);
        }
    }
}

module back_body() {
    difference() {
        union() {
            // floor and walls
            translate([0, 0, back_z])
                linear_extrude(split_z - back_z) outer2d();
        }
        // rebate that receives the lip of the front part
        translate([0, 0, split_z - lip_h]) linear_extrude(lip_h + eps)
            offset(delta = -lip_out) outer2d();
        // hollow
        translate([0, 0, back_z + floor_t])
            linear_extrude(split_z - back_z - floor_t + 3)
                offset(delta = -wall) outer2d();
        // corner columns are added back below; notch the lip around the front bosses
        for (b = bosses)
            translate([b[0], b[1], split_z - 1]) cylinder(r = boss_r + 0.4, h = 4);
        usb_cut();
    }
    // corner columns for the closing screws
    for (b = bosses)
        difference() {
            translate([b[0], b[1], back_z]) cylinder(r = boss_r, h = split_z - back_z);
            translate([b[0], b[1], back_z - eps]) cylinder(d = screw_clear, h = split_z - back_z + 2 * eps);
            translate([b[0], b[1], back_z - eps]) cylinder(d = screw_head_d, h = screw_head_depth + eps);
        }
    // posts for the board screws
    for (p = post_pos)
        difference() {
            translate([p[0], p[1], back_z + floor_t - eps])
                cylinder(r = post_r, h = board_back_z - (back_z + floor_t) + eps);
            translate([p[0], p[1], back_z - eps])
                cylinder(d = post_hole, h = board_back_z - back_z + 3);
            translate([p[0], p[1], back_z - eps])
                cylinder(d = post_head_d, h = post_head_depth + eps);
        }
}

// ----------------------------------------------------------------- output
if (part == "front") {
    // front face down on the bed
    translate([0, 0, face]) mirror([0, 0, 1]) front_part();
} else if (part == "back") {
    // floor on the bed (the back of the case is flat, ready to glue a mount to)
    translate([0, 0, -back_z]) back_part();
} else {
    // assembly preview, cut open along the right half
    difference() {
        union() {
            color("DimGray") front_part();
            color("SteelBlue") back_part();
        }
        translate([0, -200, -200]) cube([200, 400, 400]);
    }
    // ghost of the glass and the board
    color("black", 0.5) translate([0, 0, -glass_t]) linear_extrude(glass_t) rrect(glass_w, glass_h, glass_r);
    color("green", 0.4) translate([0, 0, board_back_z]) linear_extrude(-board_back_z - glass_t) rrect(glass_w - 4, glass_h - 3, 2);
}
