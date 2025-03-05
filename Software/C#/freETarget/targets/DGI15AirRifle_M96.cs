/*
 Danish DGI 15 meter target for air rifle.  Type M96
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
    class DGI15AirRifle_M96 : aTarget
    {

        private decimal pelletCaliber;// = 5.6m; //.22LR
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


        public DGI15AirRifle_M96(decimal caliber) : base(caliber)
        {
            this.pelletCaliber = caliber;
            innerTenRadiusRifle = innerRing / 2m + pelletCaliber / 2m; //4.75m;
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

        public override decimal get10Radius()
        {
            return ring10 / 2m + pelletCaliber / 2m;
        }

        public override string getName()
        {
            return typeof(DGI15AirRifle_M96).FullName;
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
    }
}
