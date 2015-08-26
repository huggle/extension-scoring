//This program is free software: you can redistribute it and/or modify
//it under the terms of the GNU General Public License as published by
//the Free Software Foundation, either version 3 of the License, or
//(at your option) any later version.

//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.

#include "scoring.hpp"
#include <wikiedit.hpp>
//#include <wikiuser.hpp>
#include <wikipage.hpp>
#include <configuration.hpp>
#include <querypool.hpp>
#include <QJsonDocument>
#include <syslog.hpp>
#include <QTimer>

scoring::scoring()
{
    this->tm = new QTimer();
    connect(this->tm, SIGNAL(timeout()), this, SLOT(Refresh()));
    this->tm->start(HUGGLE_TIMER);
}

scoring::~scoring()
{

}

bool scoring::Register()
{
    this->Init();
    return true;
}

bool scoring::IsWorking()
{
    return true;
}

void scoring::Hook_Shutdown()
{
    this->tm->stop();
    delete this->tm;
    this->SetConfig("server", this->GetConfig("server", this->GetServer()));
}

void scoring::Hook_MainWindowOnLoad(void *window)
{

}

bool scoring::Hook_EditIsReady(void *edit)
{
    if (this->Edits.contains(edit))
        return false;

    return true;
}

bool scoring::Hook_EditBeforeScore(void *edit)
{
    if (this->Edits.contains(edit))
    {
        HUGGLE_EXDEBUG1("edit " + QString::number((intptr_t) edit, 16) + " is already in list");
        return true;
    }
    Huggle::WikiEdit *WikiEdit = (Huggle::WikiEdit*)edit;
    WikiEdit->IncRef();
    Huggle::Collectable_SmartPtr<Huggle::WebserverQuery> query = new Huggle::WebserverQuery();
    query->URL = this->GetServer() + QString::number(WikiEdit->RevID) + "/";
    query->Process();
    Huggle::QueryPool::HugglePool->AppendQuery(query);
    this->Edits.insert(edit, query);
    return true;
}

void scoring::Hook_GoodEdit(void *edit)
{

}

void scoring::Refresh()
{
    foreach (void* edit, this->Edits.keys())
    {
        Huggle::Collectable_SmartPtr<Huggle::WebserverQuery> request = this->Edits[edit];
        Huggle::Collectable_SmartPtr<Huggle::WikiEdit> wiki_edit = (Huggle::WikiEdit*)edit;
        if (request->IsProcessed())
        {
            this->Edits.remove(edit);
            if (request->IsFailed())
            {
                Huggle::Syslog::HuggleLogs->ErrorLog("Scoring failed for edit " + wiki_edit->Page->PageName + ": " + request->GetFailureReason());
            } else
            {

            }
        }
    }
}

QString scoring::GetServer()
{
    return this->GetConfig("server",  "https://ores.wmflabs.org/scores/");
}

#if QT_VERSION < 0x050000
    Q_EXPORT_PLUGIN2("org.huggle.extension.qt", scoring)
#endif

