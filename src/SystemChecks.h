#ifndef SYSTEMCHECKS_H
#define SYSTEMCHECKS_H

class SystemChecks {
public:
    static bool checkCudaAvailable();

private:
    static bool s_checked;
    static bool s_isCudaAvailable;
};

#endif // SYSTEMCHECKS_H
