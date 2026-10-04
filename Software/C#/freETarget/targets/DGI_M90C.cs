/*
 Danish DGI 50 meter target for 22LR rifle.  Type M90C

 Dimensions from DGI Skyttebog 2025-26, chapter 10.1, "10-delt riffelringskive M 90":
   radius of the 10 ring   1.07 cm
   ring width              1.25 cm
   black part              14.64 cm diameter (rings 10 to 5)
   outermost ring (1)      24.64 cm diameter
   X-10                    0.89 cm diameter
 Scoring: inwards on a touched line (chapter 4.3, "Klasseprogram, riffel, 50m").

 The rulebook only describes the full 10 ring M 90. The C version is the same target where only rings 4 to 10
 are printed, so this class only draws those rings, but still scores a shot in the area of rings 3 to 1 (see getScore).
 */

using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;


namespace freETarget.targets
{
    [Serializable]
    class DGI_M90C : aTarget
    {

        private decimal pelletCaliber;// = 5.6m; //.22LR
        private const decimal targetSize = 185; //mm. a little more than the 4 ring (171.4mm), so the drawn rings fill the picture
        private const int rifleBlackRings = 5; //first ring inside the black part. rings below this are drawn black on white
        private const bool solidInnerTenRing = false;

        private const int trkZoomMin = 0;
        private const int trkZoomMax = 5;
        private const int trkZoomVal = 0;
        private const decimal pdfZoomFactor = 0.29m;

        // Ring diameters. The rulebook only gives the 10 ring and the ring width, so rings 9 to 4 are calculated:
        // each ring adds one ring width on both sides. Check: ring 5 = 146.4mm = the black part, ring 1 = 246.4mm = the outermost ring.
        private const decimal innerRing = 8.9m; //mm
        private const decimal ringWidth = 12.5m; //mm
        private const decimal ring10 = 21.4m; //mm
        private const decimal ring9 = ring10 + 2.0m * ringWidth;
        private const decimal ring8 = ring9 + 2.0m * ringWidth;
        private const decimal ring7 = ring8 + 2.0m * ringWidth;
        private const decimal ring6 = ring7 + 2.0m * ringWidth;
        private const decimal ring5 = ring6 + 2.0m * ringWidth;
        private const decimal ring4 = ring5 + 2.0m * ringWidth; //171.4mm

        // Outermost DRAWN ring. The C version only shows rings 4 to 10, so rings 3, 2 and 1 are left out of the ring list below.
        // This is not the scoring limit: getScore does not use it, and shots outside the 4 ring still count.
        private const decimal outterRing = ring4;

        private const decimal blackCircle = ring5; //146.4mm

        private decimal innerTenRadiusRifle;// = innerRing / 2m + pelletCaliber / 2m;

        private static readonly decimal[] ringsRifle = new decimal[] { ring4, ring5, ring6, ring7, ring8, ring9, ring10, innerRing };


        public DGI_M90C(decimal caliber) : base(caliber)
        {
            this.pelletCaliber = caliber;
            // Inward scoring: it is enough that the bullet hole touches the X ring, so the bullet radius is added to the ring radius.
            // 8.9 / 2 + 5.6 / 2 = 7.25mm for .22LR
            innerTenRadiusRifle = innerRing / 2m + pelletCaliber / 2m;
        }

        public override int getBlackRings()
        {
            return rifleBlackRings;
        }

        public override decimal getInnerTenRadius()
        {
            return innerTenRadiusRifle;
        }

        public override decimal getOutterRadius()
        {
            return getOutterRing() / 2m + pelletCaliber / 2m;
        }

        // Largest distance from the center to the center of the bullet that still gives a 10.
        // Plus the bullet radius because of inward scoring: a hole that touches the 10 ring line is a 10.
        // 21.4 / 2 + 5.6 / 2 = 13.5mm for .22LR
        public override decimal get10Radius()
        {
            return ring10 / 2m + pelletCaliber / 2m;
        }

        public override string getName()
        {
            return typeof(DGI_M90C).FullName;
        }

        public override decimal getOutterRing()
        {
            return outterRing;
        }

        public override decimal getProjectileCaliber()
        {
            return pelletCaliber;
        }

        public override decimal[] getRings()
        {
            return ringsRifle;
        }

        public override decimal getSize()
        {
            return targetSize;
        }

        public override decimal getZoomFactor(int value)
        {
            return (decimal)(1 / Math.Pow(2, value));
        }

        public override bool isSolidInner()
        {
            return solidInnerTenRing;
        }

        public override int getTrkZoomMaximum()
        {
            return trkZoomMax;
        }

        public override int getTrkZoomMinimum()
        {
            return trkZoomMin;
        }

        public override int getTrkZoomValue()
        {
            return trkZoomVal;
        }

        public override float getFontSize(float diff)
        {
            return diff / 6f;
        }

        public override decimal getBlackDiameter()
        {
            return blackCircle;
        }

        // Last ring that gets a number: the 9 ring is numbered, the 10 ring is not.
        public override int getRingTextCutoff()
        {
            return 9;
        }

        public override float getTextOffset(float diff, int ring)
        {
            if (ring != 3)
            {
                //return diff / 4;
                return 0;
            }
            else
            {
                return diff / 12;

            }
        }

        public override decimal getPDFZoomFactor(List<Shot> shotList)
        {
            if (shotList == null)
            {
                return pdfZoomFactor;
            }
            else
            {
                bool zoomed = true;
                foreach (Shot s in shotList)
                {
                    if (s.score < 6)
                    {
                        zoomed = false;
                    }
                }

                if (zoomed)
                {
                    return 0.5m;
                }
                else
                {
                    return 1;
                }
            }
        }


        public override int getTextRotation()
        {
            return 0;
        }

        // Value of the first (outermost) ring in the ring list. 4, because rings 3 to 1 are not drawn.
        public override int getFirstRing()
        {
            return 4;
        }

        public override (decimal, decimal) rapidFireBarDimensions()
        {
            return (-1, -1);
        }

        // The four fixed number positions are switched off: this target has its numbers at 3 angles instead (see getTextAngles).
        public override bool drawNorthText()
        {
            return false;
        }

        public override bool drawSouthText()
        {
            return false;
        }

        public override bool drawWestText()
        {
            return false;
        }

        public override bool drawEastText()
        {
            return false;
        }

        // First ring that gets a number. The 4 ring is drawn, but without a number, so the numbers are 5 to 9
        // (the upper limit is getRingTextCutoff).
        public override int getRingTextStart()
        {
            return 5;
        }

        // The ring numbers are placed 120 degrees apart: upper right (30), upper left (150) and straight down (270).
        // Degrees are counterclockwise with 0 = right, see aTarget.getTextAngles.
        public override int[] getTextAngles()
        {
            return new int[] { 30, 150, 270 };
        }

        // Score calculation for the M90 target. "radius" is the distance in mm from the center of the target to the center of the bullet.
        //
        // Scoring direction: inwards on a touched line (DGI Skyttebog 2025-26, chapter 4.3 and 10.1). A hole that touches a ring line
        // gets the HIGHER value, so the limit for each ring is ring radius PLUS bullet radius. That is already included in get10Radius().
        //
        // Why the default formula in aTarget (11 - radius / get10Radius()) can not be used:
        // it assumes that all rings have the same width as the 10 ring. With the bullet radius added the 10 zone is 13.5mm wide,
        // but the other rings are 12.5mm wide, so the default formula would give too high values, more for each ring outwards.
        // The score is therefore calculated in two parts (same pattern as Running10m and Pistol25mRF):
        //   inside the 10 zone:   11.0 in the center, falling to 10.0 at the 10 limit
        //   outside the 10 zone:  falling 1.0 for every ring width (12.5mm)
        //
        // Rings 3 to 1 are not drawn on the C version, but the formula continues outside the 4 ring, so shots there still count
        // 3, 2 and 1. Outside the 1 ring the result is below 1, which Shot.computeScore turns into 0.
        // Limits for .22LR (5.6mm): 10 <= 13.5mm, 9 <= 26.0mm, then 12.5mm per ring out to 1 <= 126.0mm.
        public override decimal getScore(decimal radius)
        {
            if (radius > get10Radius())
            {
                return 10 - (radius - get10Radius()) / ringWidth;
            }
            else
            {
                return 11 - (radius / get10Radius());
            }
        }
    }
}
