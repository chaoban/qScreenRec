#include "core/license.h"

#include <QCoreApplication>

namespace {
class FullLicense : public LicenseProvider
{
public:
    QString editionName() const override
    {
        return QCoreApplication::translate("License", "Full edition");
    }
    bool hasFeature(Feature) const override { return true; }
};
} // namespace

const LicenseProvider &License::provider()
{
    static FullLicense full;
    return full;
}
