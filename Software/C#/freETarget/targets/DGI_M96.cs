/*
 Danish DGI 15 meter target for air rifle.  Type M96
 */

using System;
using System.Collections.Generic;


namespace freETarget.targets
{
    [Serializable]
    class DGI_M96 : aTarget
    {

        private decimal pelletCaliber;// = 4.5; // air rifle
        private const decimal targetSize = 100; //mm
        private const int rifleBlackRings = 6;
        private const bool solidInnerTenRing = false;

        private const int trkZoomMin = 0;
        private const int trkZoomMax = 5;
        private const int trkZoomVal = 0;
        private const decimal pdfZoomFactor = 0.29m;

        private const decimal innerRing = 7.5m; //mm
        private const decimal ringWidth = 3.75m; //mm
        private const decimal ring10 = 12.74m; //mm
        private const decimal ring9 = ring10 + 2.0m * ringWidth;
        private const decimal ring8 = ring9 + 2.0m * ringWidth;
        private const decimal ring7 = ring8 + 2.0m * ringWidth;
        private const decimal ring6 = ring7 + 2.0m * ringWidth;
        private const decimal ring5 = ring6 + 2.0m * ringWidth;
        private const decimal ring4 = ring5 + 2.0m * ringWidth;
        private const decimal ring3 = ring4 + 2.0m * ringWidth;
        private const decimal ring2 = ring3 + 2.0m * ringWidth;
        private const decimal outterRing = ring2 + 2.0m * ringWidth;

        private const decimal blackCircle = ring6; //mm

        private decimal innerTenRadiusRifle;// = innerRing / 2m + pelletCaliber / 2m; 

        private static readonly decimal[] ringsRifle = new decimal[] { outterRing, ring2, ring3, ring4, ring5, ring6, ring7, ring8, ring9, ring10, innerRing };


        public DGI_M96(decimal caliber) : base(caliber)
        {
            this.pelletCaliber = caliber;
            // Outward scoring (see getScore): the whole pellet hole must be inside the X ring,
            // so the pellet radius is subtracted from the ring radius instead of added.
            // 7.5 / 2 - 4.5 / 2 = 1.5mm for a 4.5mm pellet
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

        // Largest distance from the center to the center of the pellet that still scores.
        // Minus the pellet radius because of outward scoring: a hole that touches the outermost ring is a 0.
        public override decimal getOutterRadius()
        {
            return getOutterRing() / 2m - pelletCaliber / 2m;
        }

        // Largest distance from the center to the center of the pellet that still gives a 10.
        // Minus the pellet radius because of outward scoring: a hole that touches the 10 ring line is a 9.
        public override decimal get10Radius()
        {
            return ring10 / 2m - pelletCaliber / 2m;
        }

        public override string getName()
        {
            return typeof(DGI_M96).FullName;
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
            return 10;
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
            return 1;
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

        // Score calculation for the M96/G target. "radius" is the distance in mm from the center of the target to the center of the pellet.
        //
        // Why the default formula in aTarget (11 - radius / get10Radius()) can not be used:
        //  1. Scoring direction. DGI Skyttebog 2025-26, chapter 10.1: all ring targets are scored inwards on a touched line,
        //     except M96/G and M84, which are designed for outward scoring ("udadtaelling ved beroert streg").
        //     A hole that touches a ring line gets the LOWER value, so the whole hole must be inside the ring.
        //     The limit for each ring is therefore ring radius MINUS pellet radius (inward scoring uses plus).
        //     A hole that touches the outermost ring is outside the target and gives 0.
        //  2. Ring spacing. The default formula assumes that all rings have the same width as the 10 ring.
        //     On the M96 the 10 ring has a radius of 6.37mm and the other rings are 3.75mm wide,
        //     so a linear formula gives wrong values from the 9 ring and out.
        //
        // The integer part is found by walking the rings from the 10 and outwards. The decimal is interpolated inside the ring
        // that was hit: .9 next to the inner line, .0 next to the outer line.
        // Limits for a 4.5mm pellet: 10 < 4.12mm, 9 < 7.87mm, then 3.75mm per ring out to 1 < 37.87mm, otherwise 0.
        public override decimal getScore(decimal radius)
        {
            decimal[] rings = new decimal[] { ring10, ring9, ring8, ring7, ring6, ring5, ring4, ring3, ring2, outterRing }; //ring diameters, from the center and out
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

            return 0; //outside the 1 ring, or touching it
        }
    }
}
