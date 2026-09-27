#include "actuator_app.h"

int main(void)
{
    if (ActuatorApp_Init() != 1U)
    {
        while (1)
        {
        }
    }

    while (1)
    {
        ActuatorApp_Run();
    }
}