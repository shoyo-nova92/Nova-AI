#include "goal_decomposer.hpp"

#include <iostream>

using namespace std;

int main()
{
    GoalDecomposer decomposer;

    const string goal =
        "Create a Java project on my desktop, "
        "add a basic Main.java class, "
        "and open the project in VS Code.";

    cout << "[GOAL]" << endl;
    cout << goal << endl;

    MultiStepPlan plan =
        decomposer.decompose(goal);

    cout << endl;
    cout << "[RESULT]" << endl;

    cout << "Valid: "
         << (plan.valid ? "true" : "false")
         << endl;

    cout << "Message: "
         << plan.message
         << endl;

    cout << "Step Count: "
         << plan.steps.size()
         << endl;

    for (const auto& step : plan.steps)
    {
        cout << endl;

        cout << "Step ID: "
             << step.id
             << endl;

        cout << "Description: "
             << step.description
             << endl;

        cout << "Dependencies: ";

        if (step.dependencies.empty())
        {
            cout << "[none]";
        }
        else
        {
            for (const auto& dependency :
                 step.dependencies)
            {
                cout << dependency << " ";
            }
        }

        cout << endl;
    }

    return plan.valid ? 0 : 1;
}