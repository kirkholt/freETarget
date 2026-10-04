/*
 Danish DGI 15 meter target for 22LR rifle.  Type M84
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
    class DGI_M84 : aTarget
    {

        private decimal pelletCaliber;// = 5.6m; //.22LR
        private const decimal targetSize = 100; //mm
        private const int rifleBlackRings = 9;
        private const bool solidInnerTenRing = false;

        private const int trkZoomMin = 0;
        private const int trkZoomMax = 5;
        private const int trkZoomVal = 0;
        private const decimal pdfZoomFactor = 0.29m;


        private const decimal ring6 = 86.0m; //mm
        private const decimal ring7 = 66.0m; //mm
        private const decimal ring8 = 46.0m; //mm
        private const decimal ring9 = 26.0m; //mm
        private const decimal ring10 = 13.0m; //mm
        private const decimal innerRing = 9.0m; //mm

        private const decimal outterRing = ring6;
        private const decimal blackCircle = 41m; //mm

        private decimal innerTenRadiusRifle;// = innerRing / 2m + pelletCaliber / 2m; 

        private static readonly decimal[] ringsRifle = new decimal[] { ring6, ring7, ring8, ring9, ring10, innerRing };


        public DGI_M84(decimal caliber) : base(caliber)
        {
            this.pelletCaliber = caliber;
            // Outward scoring (see getScore): the whole bullet hole must be inside the X ring,
            // so the bullet radius is subtracted from the ring radius instead of added.
            // 9.0 / 2 - 5.6 / 2 = 1.7mm for .22LR
            innerTenRadiusRifle = innerRing / 2m - pelletCaliber / 2m;
        }

        public override int getBlackRings()
        {
            return rifleBlackRings;
        }

        public override decimal getInnerTenRadius()
        {
            return innerTenRadiusRifle;
        }

        // Largest distance from the center to the center of the bullet that still scores.
        // Minus the bullet radius because of outward scoring: a hole that touches the outermost ring is a 0.
        public override decimal getOutterRadius()
        {
            return getOutterRing() / 2m - pelletCaliber / 2m;
        }

        // Largest distance from the center to the center of the bullet that still gives a 10.
        // Minus the bullet radius because of outward scoring: a hole that touches the 10 ring line is a 9.
        public override decimal get10Radius()
        {
            return ring10 / 2m - pelletCaliber / 2m;
        }

        public override string getName()
        {
            return typeof(DGI_M84).FullName;
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

        public override int getRingTextCutoff()
        {
            return 7;
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

        public override int getFirstRing()
        {
            return 6;
        }

        public override (decimal, decimal) rapidFireBarDimensions()
        {
            return (-1, -1);
        }

        public override bool drawNorthText()
        {
            return true;
        }

        public override bool drawSouthText()
        {
            return true;
        }

        public override bool drawWestText()
        {
            return true;
        }

        public override bool drawEastText()
        {
            return true;
        }

        // Score calculation for the M84 target. "radius" is the distance in mm from the center of the target to the center of the bullet.
        //
        // Why the default formula in aTarget (11 - radius / get10Radius()) can not be used:
        //  1. Scoring direction. DGI Skyttebog 2025-26, chapter 10.1: all ring targets are scored inwards on a touched line,
        //     except M96/G and M84, which are designed for outward scoring ("udadtaelling ved beroert streg").
        //     A hole that touches a ring line gets the LOWER value, so the whole hole must be inside the ring.
        //     The limit for each ring is therefore ring radius MINUS bullet radius (inward scoring uses plus).
        //     A hole that touches the outermost ring is outside the target and gives 0 (chapter 4: lowest shot value is 6).
        //  2. Ring spacing. The default formula assumes that all rings have the same width as the 10 ring.
        //     On the M84 the 10 and 9 rings are 6.5mm wide and the 8, 7 and 6 rings are 10mm wide,
        //     so a linear formula gives wrong values from the 8 ring and out.
        //
        // The integer part is found by walking the rings from the 10 and outwards. The decimal is interpolated inside the ring
        // that was hit: .9 next to the inner line, .0 next to the outer line.
        // Limits for .22LR (5.6mm): 10 < 3.7mm, 9 < 10.2mm, 8 < 20.2mm, 7 < 30.2mm, 6 < 40.2mm, otherwise 0.
        public override decimal getScore(decimal radius)
        {
            decimal[] rings = new decimal[] { ring10, ring9, ring8, ring7, ring6 }; //ring diameters, from the center and out
            decimal inner = 0; //limit of the previous (higher) ring
            for (int i = 0; i < rings.Length; i++)
            {
                decimal outer = rings[i] / 2m - pelletCaliber / 2m; //the whole hole must be inside this ring
                if (radius < outer) //strictly less: touching the line gives the lower value
                {
                    decimal fraction = (outer - radius) / (outer - inner);
                    if (i > 0 && fraction > 0.99m)
                    {
                        // A hole that exactly touches the inner line gives fraction = 1, which would round the score up to the next ring.
                        // Outward scoring gives it the lower value, so the decimal is kept below 1.
                        // Not done for the 10 ring (i == 0): a center shot must give 11, which Shot.computeScore limits to 10.9.
                        fraction = 0.99m;
                    }
                    return (10 - i) + fraction;
                }
                inner = outer;
            }

            return 0; //outside the 6 ring, or touching it
        }
    }
}
